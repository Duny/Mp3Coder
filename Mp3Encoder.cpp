#include "StdAfx.h"
#include "Mp3Utils.h"
#include "Mp3Encoder.h"

static const UInt32 kMainStreamSize = 1 << 20;
static const UInt32 kHeaderStreamSize = 1 << 18;
static const UInt32 kSizeInfoStreamSize = 1 << 18;
static const UInt32 kInStreamSize = 1 << 20;

namespace NCompress {
namespace NMp3 {

bool CEncoder::InitBuffers ()
{
	return m_mainBuffer.Create (kMainStreamSize) && m_headersBuffer.Create (kHeaderStreamSize) &&
        m_sideInfoBuffer.Create (kSizeInfoStreamSize) && m_inBuffer.Create (kInStreamSize);
}

HRESULT CEncoder::Flush ()
{
    RINOK (m_mainBuffer.Flush ());
    RINOK (m_headersBuffer.Flush ());
    return m_sideInfoBuffer.Flush ();
}

bool CEncoder::FindFrameHeader (UInt32 *posOut, FrameHeader *outHeader)
{
    Byte buf[MP3_FRAME_HEADER_SIZE];
    FrameHeader header;

    while (1) {
        // find sync word
        if (!m_inBuffer.ReadByte (buf[0]))
            return false;
        if (buf[0] != 0xFF) {
            m_mainBuffer.WriteByte (buf[0]);
            continue;
        }
        if (!m_inBuffer.ReadByte (buf[1])) {
            m_mainBuffer.WriteByte (buf[0]);
            return false;
        }
        if ((buf[1] & 0xE0) != 0xE0) {
            m_mainBuffer.WriteBytes (buf, 2);
            continue;
        }
        UInt32 len = m_inBuffer.ReadBytes (buf + 2, 2);
        if (len != 2) {
            m_mainBuffer.WriteBytes (buf, 2 + len);
            return false;
        }

       // try to decode frame header
        if (header.InitFromMem (buf)) {
            if (posOut) *posOut = (UInt32)m_inBuffer.GetProcessedSize () - MP3_FRAME_HEADER_SIZE;
            if (outHeader) *outHeader = header;
            return true;
        }
        else
            m_mainBuffer.WriteBytes (buf, 4);
    }
    return false;
}

HRESULT CEncoder::CodeFrames ()
{
    Byte buf[1024 * 2];

    UInt32 curFrameOffset   = (UInt32)-1,
           prevFrameOffset  = (UInt32)-1,
           prevFrameSize    = (UInt32)-1, 
           firstFrameOffset = (UInt32)-1,
           frameCounter     = 0;

    FrameHeader curFrame, prevFrame;

    BadCRCVector badCRC;
    SpaceAfterFrameVector spaceAfterFrame;

    FrameHeaderCollector hdrCollector;
	SideInfoCollector    siCollector;

    for (; ;frameCounter++) {
        if (!FindFrameHeader (&curFrameOffset, &curFrame))
            break;

        if (frameCounter == 0)
            firstFrameOffset = curFrameOffset;

        // read entire frame
        UInt32 len = m_inBuffer.ReadBytes (buf, curFrame.m_frameSize);
        if (len != curFrame.m_frameSize) { // last frame is truncated
			m_mainBuffer.WriteBytes (curFrame.m_data, MP3_FRAME_HEADER_SIZE);
            m_mainBuffer.WriteBytes (buf, len);
            break;
        }

        // current frame should be right after prev frame
        // if it is not, then we must remember this fact
        if (prevFrameOffset != (UInt32)-1) {
			if (curFrameOffset != prevFrameOffset + prevFrameSize) {
                spaceAfterFrame.Add (SpaceAfterFrame (frameCounter - 1, 
                    curFrameOffset - prevFrameOffset - prevFrameSize));
			}
        }

        // collect stats
        hdrCollector.CollectData (curFrame);

        Byte *ptr = buf;

        // check CRC
        bool invalidCRC = false;
        if (curFrame.m_protection) {
            WORD crc = ptr[1] | (ptr[0] << 8);
            ptr += 2;
		    invalidCRC = CalcCRC16 (&curFrame.m_data[2], ptr, curFrame.m_protectedBits) != crc;
            if (invalidCRC)
                badCRC.Add (BadCRC (frameCounter, (crc >> 8) & 0xFF, crc & 0xFF));
            
        }

        // code side info
		siCollector.CollectData (ptr, curFrame.m_sideInfoSize);
        ptr += curFrame.m_sideInfoSize;

        // code rest of the frame
        m_mainBuffer.WriteBytes (ptr, curFrame.m_frameSize - (ptr - buf));
        
        if (frameCounter % 50 == 0) {
            RINOK (ShowProgress ());
        }

        prevFrameSize = curFrame.m_frameSize + MP3_FRAME_HEADER_SIZE;
        prevFrameOffset = curFrameOffset;
        prevFrame = curFrame;
    }

    // write side info
    siCollector.WriteData (m_sideInfoBuffer);

    //
    // write headers

    // write codec version
    m_headersBuffer.WriteByte (g_codecVersion);

    // write common info
    WriteUInt32 (frameCounter, m_headersBuffer);
	if (frameCounter == 0)
        return S_OK;
    WriteUInt32 (firstFrameOffset, m_headersBuffer);

    hdrCollector.WriteData (m_headersBuffer, frameCounter);

    CodeBadCRC (badCRC);
    CodeSpaceAfterFrame (spaceAfterFrame);
    
	return S_OK;
}

void CEncoder::CodeBadCRC (const BadCRCVector &v)
{
    int n = v.Size ();
    WriteUInt32 ((UInt32)n, m_headersBuffer);
    for (int i = 0; i < n; i++) {
		WriteUInt32 (v[i].m_frameNum, m_headersBuffer);
        m_headersBuffer.WriteBytes (v[i].m_crc, 2);
    }
}

void CEncoder::CodeSpaceAfterFrame (const SpaceAfterFrameVector &v)
{
    int n = v.Size ();
    WriteUInt32 ((UInt32)n, m_headersBuffer);
    for (int i = 0; i < n; i++) {
		WriteUInt32 (v[i].m_frameNo, m_headersBuffer);
        WriteUInt32 (v[i].m_size, m_headersBuffer);
    }
}

HRESULT CEncoder::CodeReal (ISequentialInStream **inStreams,
    const UInt64 **inSizes,
    UInt32 numInStreams,
    ISequentialOutStream **outStreams,
    const UInt64 ** /* outSizes */,
    UInt32 numOutStreams,
    ICompressProgressInfo *progress)
{
    if (numInStreams != 1 || numOutStreams != 3)
        return E_INVALIDARG;

    if (!InitBuffers ())
        return E_OUTOFMEMORY;

    m_progress = progress;

    m_inBuffer.SetStream (inStreams[0]); m_inBuffer.Init ();
    m_mainBuffer.SetStream (outStreams[0]); m_mainBuffer.Init ();
	m_headersBuffer.SetStream (outStreams[1]);	m_headersBuffer.Init ();
    m_sideInfoBuffer.SetStream (outStreams[2]); m_sideInfoBuffer.Init ();

    CCoderReleaser releaser (this);
	CodeFrames ();

	return S_OK;
}

STDMETHODIMP CEncoder::Code (ISequentialInStream **inStreams,
    const UInt64 **inSizes,
    UInt32 numInStreams,
    ISequentialOutStream **outStreams,
    const UInt64 **outSizes,
    UInt32 numOutStreams,
    ICompressProgressInfo *progress)
{
    try {
        return CodeReal (inStreams, inSizes, numInStreams, outStreams, outSizes,numOutStreams, progress);
    }
    catch (const COutBufferException &e) { return e.ErrorCode; }
    catch (...) { return S_FALSE; }
}
}}