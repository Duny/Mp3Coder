#include "StdAfx.h"
#include "Mp3Utils.h"
#include "Mp3Decoder.h"

static const UInt32 kMainStreamSize = 1 << 20;
static const UInt32 kHeaderStreamSize = 1 << 18;
static const UInt32 kSizeInfoStreamSize = 1 << 18;
static const UInt32 kInStreamSize = 1 << 20;

namespace NCompress {
namespace NMp3 {

bool CDecoder::InitBuffers ()
{
    return m_mainBuffer.Create (kMainStreamSize) && m_headersBuffer.Create (kHeaderStreamSize) &&
        m_sideInfoBuffer.Create (kSizeInfoStreamSize) && m_outBuffer.Create (kInStreamSize);
}

HRESULT CDecoder::DecodeFrames ()
{
    Byte buf[1024 * 16];
    UInt32 numFrames ((UInt32)-1),
        firstFrameOffset ((UInt32)-1);
    
    BadCRCVector badCRCs;
    SpaceAfterFrameVector spaces;

    FrameHeaderCollector hdrCollector;

    //
    // decode headers

    // read codec version
	if (m_headersBuffer.ReadByte () != g_codecVersion) return S_FALSE;

    // total number of frames
	if ((numFrames = ReadUInt32 (m_headersBuffer)) == 0) { // not mp3 file
        TransferBytes (&m_mainBuffer, &m_outBuffer, (UInt32)-1);
        return S_OK;
    }
    // prepanded tag len
    firstFrameOffset = ReadUInt32 (m_headersBuffer);

    // read header field flags and values
    hdrCollector.ReadData (m_headersBuffer, numFrames);
	
    DecodeBadCRC (badCRCs);
    DecodeSpaceAfterFrame (spaces);

    //
    // decode frames
    //

	// decode preppend tag
    TransferBytes (&m_mainBuffer, &m_outBuffer, firstFrameOffset);

	// decode frames
	FrameHeader header;

	for (UInt32 i = 0; i < numFrames; i++) {
        if (!hdrCollector.GetNextHeader (header))
            return false;
        m_outBuffer.WriteBytes (header.m_data, MP3_FRAME_HEADER_SIZE);

		Byte *ptr = buf;

		// check crc
		bool sideInfoWasRead = false;
		if (header.m_protection) {
            int badCRCIndex = -1;
            for (int j = 0, n = badCRCs.Size (); j < n; j++) {
                if (badCRCs[j].m_frameNum == i) {
                    badCRCIndex = j;
                    break;
                }
            }

            if (badCRCIndex != -1) {
                ptr[0] = badCRCs[badCRCIndex].m_crc[0];
                ptr[1] = badCRCs[badCRCIndex].m_crc[1];
                badCRCs.Delete (badCRCIndex);
            }
            else {
                Byte *sideInfo = ptr + 2;
                m_sideInfoBuffer.ReadBytes (sideInfo, header.m_sideInfoSize);
				WORD crc = CalcCRC16 (&header.m_data[2], sideInfo, header.m_protectedBits);
				ptr[0] = (crc >> 8) & 0xFF;
				ptr[1] = crc & 0xFF;
				ptr += header.m_sideInfoSize;
				sideInfoWasRead = true;
			}
			ptr += 2;
		}

		// read side info
		if (!sideInfoWasRead) {
			m_sideInfoBuffer.ReadBytes (ptr, header.m_sideInfoSize);
			ptr += header.m_sideInfoSize;
		}

		// write out header, crc(if peresent) and side info
		size_t sz = (size_t)ptr - (size_t)buf;
		m_outBuffer.WriteBytes (buf, sz);

		// write rest of the frame
		TransferBytes (&m_mainBuffer, &m_outBuffer, header.m_frameSize - sz);

        for (int j = 0, n = spaces.Size (); j < n; j++) {
            if (spaces[j].m_frameNo == i) {
                TransferBytes (&m_mainBuffer, &m_outBuffer, spaces[j].m_size);
                spaces.Delete (j);
                break;
            }
        }

        if (i % 50 == 0) {
            RINOK (ShowProgress ());
        }
	}

	// decode append tag
    TransferBytes (&m_mainBuffer, &m_outBuffer, (UInt32)-1);

    return S_OK;
}

void CDecoder::DecodeBadCRC (BadCRCVector &vOut)
{
    BadCRC badCRC;
    UInt32 n = ReadUInt32 (m_headersBuffer);
    vOut.Reserve (n);
    for (UInt32 i = 0; i < n; i++) {
        badCRC.m_frameNum = ReadUInt32 (m_headersBuffer);
        m_headersBuffer.ReadBytes (badCRC.m_crc, 2);
		vOut.Add (badCRC);
    }
}

void CDecoder::DecodeSpaceAfterFrame (SpaceAfterFrameVector &vOut)
{
    SpaceAfterFrame space;
    UInt32 n = ReadUInt32 (m_headersBuffer);
    vOut.Reserve (n);
    for (UInt32 i = 0; i < n; i++) {
        space.m_frameNo = ReadUInt32 (m_headersBuffer);
		space.m_size = ReadUInt32 (m_headersBuffer);
		vOut.Add (space);
    }
}

HRESULT CDecoder::CodeReal (ISequentialInStream **inStreams,
    const UInt64 **inSizes/* inSizes */,
    UInt32 numInStreams,
    ISequentialOutStream **outStreams,
    const UInt64 ** /* outSizes */,
    UInt32 numOutStreams,
    ICompressProgressInfo *progress)
{
    if (numInStreams != 3 || numOutStreams != 1)
        return E_INVALIDARG;

    if (!InitBuffers ())
        return E_OUTOFMEMORY;

    m_progress = progress;

    m_outBuffer.SetStream (outStreams[0]); m_outBuffer.Init ();
    m_mainBuffer.SetStream (inStreams[0]); m_mainBuffer.Init ();
    m_headersBuffer.SetStream (inStreams[1]); m_headersBuffer.Init ();
    m_sideInfoBuffer.SetStream (inStreams[2]); m_sideInfoBuffer.Init ();

    CCoderReleaser releaser (this);

    return DecodeFrames ();
}

STDMETHODIMP CDecoder::Code (ISequentialInStream **inStreams,
    const UInt64 **inSizes,
    UInt32 numInStreams,
    ISequentialOutStream **outStreams,
    const UInt64 **outSizes,
    UInt32 numOutStreams,
    ICompressProgressInfo *progress)
{
    try {
        return CodeReal(inStreams, inSizes, numInStreams, outStreams, outSizes,numOutStreams, progress);
    }
    catch (const CInBufferException &e) { return e.ErrorCode; }
    catch (const COutBufferException &e) { return e.ErrorCode; }
    catch (...) { return S_FALSE; }
}
}}