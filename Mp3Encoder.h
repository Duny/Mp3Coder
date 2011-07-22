#ifndef __MP3_ENCODER_H
#define __MP3_ENCODER_H

#include "Mp3Frame.h"

namespace NCompress {
namespace NMp3 {

const Byte g_codecVersion = 1;

struct BadCRC
{
    UInt32 m_frameNum;
    Byte   m_crc[2];

    BadCRC (UInt32 frameNum, Byte b1, Byte b2) : m_frameNum (frameNum) {
        m_crc[0] = b1; m_crc[1] = b2;
    }
    BadCRC () {}
};
typedef CObjectVector<BadCRC> BadCRCVector;

struct SpaceAfterFrame
{
    UInt32 m_frameNo; // # of frame after that there is some space
    UInt32 m_size;

    SpaceAfterFrame (UInt32 frameNo, UInt32 size) : m_frameNo (frameNo), m_size (size) {}
    SpaceAfterFrame () {}
};
typedef CObjectVector<SpaceAfterFrame> SpaceAfterFrameVector;

class CEncoder:
	public ICompressCoder2,
	public CMyUnknownImp
{
    CInBuffer m_inBuffer;
    COutBuffer m_mainBuffer, m_sideInfoBuffer, m_headersBuffer;

    ICompressProgressInfo *m_progress;
    HRESULT ShowProgress () {
        if (m_progress) {
            UInt64 curPos = m_inBuffer.GetProcessedSize ();
            RINOK (m_progress->SetRatioInfo (&curPos, NULL));
        }
        return S_OK;
    }

    class CCoderReleaser
    {
        CEncoder *m_coder;
    public:
        CCoderReleaser (CEncoder *coder) : m_coder (coder) {}
        ~CCoderReleaser () {  m_coder->ReleaseStreams (); }
    };

    void ReleaseStreams () {
        Flush ();
        m_inBuffer.ReleaseStream ();
        m_mainBuffer.ReleaseStream ();
		m_headersBuffer.ReleaseStream ();
        m_sideInfoBuffer.ReleaseStream ();
    }

    bool InitBuffers ();
    HRESULT Flush ();

	bool FindFrameHeader (UInt32 *posOut, FrameHeader *outHeader);

    HRESULT CodeFrames ();
    void CodeBadCRC (const BadCRCVector &v);
    void CodeSpaceAfterFrame (const SpaceAfterFrameVector &v);

public:
    MY_UNKNOWN_IMP

    HRESULT CodeReal (ISequentialInStream **inStreams,
        const UInt64 **inSizes,
        UInt32 numInStreams,
        ISequentialOutStream **outStreams,
        const UInt64 **outSizes,
        UInt32 numOutStreams,
        ICompressProgressInfo *progress);
    STDMETHOD(Code)(ISequentialInStream **inStreams,
        const UInt64 **inSizes,
        UInt32 numInStreams,
        ISequentialOutStream **outStreams,
        const UInt64 **outSizes,
        UInt32 numOutStreams,
        ICompressProgressInfo *progress);
};
}};
#endif