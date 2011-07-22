#include "StdAfx.h"
#include "Mp3Frame.h"

namespace NCompress {
namespace NMp3 {

static UInt32 g_tabSel[2][3][16] = {
   { { 0,32,64,96,128,160,192,224,256,288,320,352,384,416,448 },
     { 0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384 },
     { 0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320 }},

   { { 0,32,48,56,64,80,96,112,128,144,160,176,192,224,256 },
     { 0,8,16,24,32,40,48,56,64,80,96,112,128,144,160 },
     { 0,8,16,24,32,40,48,56,64,80,96,112,128,144,160 } }
};

static UInt32 g_freqs[9] = { 44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000 }; 

// size of side information (only for Layer III)
// 1. index = LSF, 2. index = mono/stereo
static Byte g_sideInfoSizes[2][2] = { { 17, 32 }, { 9, 17 } };

static const Byte g_fieldLen[totalHeaderFields] = { 2, 2, 1, 4, 2, 1, 1, 2, 2, 1, 1, 2 };
static const Byte g_reservedFieldValue[totalHeaderFields] = { 0, 0, 0xFF, 0xF, 0x3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2 };

bool FrameHeader::InitFromMem (Byte *header)
{
	if ((header[0] != 0xFF) || ((header[1] & 0xE0) != 0xE0))
		return false; // first 11 bits must be '1'

	for (int i = 0; i < MP3_FRAME_HEADER_SIZE; i++)
        m_data[i] = header[i];

	CBitReaderMem iter;
	iter.Init (header);

	// skip sync word
	iter.ReadBits (8);
	iter.ReadBits (3);

	for (int i = 0; i < totalHeaderFields; i++) {
		Byte val = iter.ReadBits (g_fieldLen[i]);
		if (g_reservedFieldValue[i] != 0xFF && val == g_reservedFieldValue[i])
			return false;
		m_fields[i] = val;
	}

    return ComputeData ();
}

bool FrameHeader::InitFromByteBuf (const CByteBuffer & buf)
{
	CBitWriterMem bw;
	bw.Init (m_data);

	bw.WriteBits (0xFF, 8);
	bw.WriteBits (0x7, 3);

    for (int i = 0; i < totalHeaderFields; i++) {
        m_fields[i] = buf[i];
		bw.WriteBits (m_fields[i], g_fieldLen[i]);
    }

    return ComputeData ();
}

bool FrameHeader::ComputeData ()
{
    if (m_fields[layer] != 1) // "1" for Layer III
        return false;

    m_protection = !m_fields[protectionBit];

    Byte stereo = m_fields[channelMode] != MPG_MD_MONO;
	Byte lsf = (m_fields[mpegVersion] == 0) || (m_fields[mpegVersion] == 2);
	Byte freqIndex = m_fields[mpegVersion] == 0 ? 6 + m_fields[frequency] : m_fields[frequency] + (lsf * 3);

	// calculate frame size
    m_frameSize  = g_tabSel[lsf][2][m_fields[bitrateIndex]] * 144000;
    m_frameSize /= g_freqs[freqIndex] << lsf;
	if (!m_frameSize) return false;

    m_frameSize += m_fields[paddingBit] - MP3_FRAME_HEADER_SIZE;

	m_sideInfoSize = g_sideInfoSizes[lsf][stereo];

	// calculate number of protected bits (for CRC)
    m_protectedBits = (m_sideInfoSize << 3) + 8 * 2;
	
    return true;
}

FrameHeaderCollector::FrameHeaderCollector () : m_isEmpty (true)
{
    for (int i = 0; i < totalHeaderFields; i++) {
        m_flags[i] = true;
        m_writers[i].Init (&m_fields[i]);
    }
}

void FrameHeaderCollector::CollectData (FrameHeader & header)
{
    if (m_isEmpty) {
        m_fields0 = header.m_fields;
        m_isEmpty = false;
    }

    for (int i = 0; i < totalHeaderFields; i++) {
        m_writers[i].WriteBits (header.m_fields[i], g_fieldLen[i]);
	    if (m_flags[i] && (header.m_fields[i] != m_fields0[i]))
			m_flags[i] = false;
	}
}

void FrameHeaderCollector::ReadData (CInBuffer & buf, UInt32 numFrames)
{
    UInt16 flags;

    flags = buf.ReadByte ();
    flags |= (buf.ReadByte () << 8);

    m_fields0.SetCapacity (totalHeaderFields);

    for (UInt32 i = 0; i < totalHeaderFields; i++) {
        m_flags[i] = flags & (1 << i) ? true : false;
        if (m_flags[i])
            m_fields0[i] = buf.ReadByte ();
        else {
            m_fields0[i] = 0;

            UInt32 numBytes = (numFrames * g_fieldLen[i] + 7) >> 3;
            m_fields[i].Reserve (numBytes);
            for (UInt32 j = 0; j < numBytes; j++)
                m_fields[i].Add (buf.ReadByte ());
        }
        m_readers[i].Init (&m_fields[i]);
    }
}

void FrameHeaderCollector::WriteData (COutBuffer & buf, UInt32 numFrames)
{
    UInt16 flags = 0;

    for (int i = 0; i < totalHeaderFields; i++) {
        if (m_flags[i])
            flags |= 1 << i;
    }
    buf.WriteByte (flags & 0xFF);
    buf.WriteByte ((flags >> 8) & 0xFF);

    for (int i = 0; i < totalHeaderFields; i++) {
		if (m_flags[i])
            buf.WriteByte (m_fields0[i]);
        else {
			m_writers[i].Flush ();
            buf.WriteBytes (&m_fields[i][0], m_fields[i].Size ());
        }
	}
}

bool FrameHeaderCollector::GetNextHeader (FrameHeader & headerOut)
{
    CByteBuffer buf (totalHeaderFields);
    for (int i = 0; i < totalHeaderFields; i++)
        buf[i] = m_flags[i] ? m_fields0[i] : m_readers[i].ReadBits (g_fieldLen[i]);

    return headerOut.InitFromByteBuf (buf);
}

//void FrameHeaderCollector::GetSideInfoSizes (UInt32 numFrames, CByteVector &sizes)
//{
//    Byte stereo, lsf;
//    bool channelModeFlag = m_flags[channelMode],
//         mpegVersionFlag = m_flags[mpegVersion];
//
//    if (channelModeFlag)
//        stereo = m_fields0[channelMode] != MPG_MD_MONO;
//    if (mpegVersionFlag)
//        lsf = (m_fields0[mpegVersion] == 0) || (m_fields0[mpegVersion] == 2);
//
//	sizes.Reserve (numFrames);
//    for (UInt32 i = 0; i < numFrames; i++) {
//        if (!channelModeFlag)
//            stereo = m_readers[channelMode].ReadBits (g_fieldLen[channelMode]) != MPG_MD_MONO;
//
//        if (!mpegVersionFlag) {
//            Byte version = m_readers[mpegVersion].ReadBits (g_fieldLen[mpegVersion]);
//            lsf = (version == 0) || (version == 2);
//        }
//
//        sizes.Add (g_sideInfoSizes[lsf][stereo]);
//    }
//
//    // reinit readers
//    if (!channelModeFlag)
//	    m_readers[channelMode].Init (&m_fields[channelMode]);
//    if (!mpegVersionFlag)
//        m_readers[mpegVersion].Init (&m_fields[mpegVersion]);
//}
}}