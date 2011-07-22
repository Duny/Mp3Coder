#ifndef __MP3_DECODER_H
#define __MP3_DECODER_H

#include "CPP/Common/MyCom.h"
#include "CPP/7zip/ICoder.h"
#include "CPP/7zip/Common/OutBuffer.h"
#include "CPP/7zip/Common/InBuffer.h"

#include "Mp3Encoder.h"

namespace NCompress {
namespace NMp3 {

class CDecoder:
    public ICompressCoder2,
    public CMyUnknownImp
{
    COutBuffer m_outBuffer;
    CInBuffer m_mainBuffer, m_sideInfoBuffer, m_headersBuffer;

    ICompressProgressInfo *m_progress;
    HRESULT ShowProgress () {
        if (m_progress) {
            UInt64 curPos = m_outBuffer.GetProcessedSize ();
            RINOK (m_progress->SetRatioInfo (NULL, &curPos));
        }
        return S_OK;
    }

    class CCoderReleaser
    {
        CDecoder *m_coder;
    public:
        CCoderReleaser (CDecoder *coder): m_coder (coder) {}
        ~CCoderReleaser () {  m_coder->ReleaseStreams (); }
    };

    void ReleaseStreams () {
        m_outBuffer.Flush ();
        m_outBuffer.ReleaseStream ();
        m_mainBuffer.ReleaseStream ();
        m_headersBuffer.ReleaseStream ();
        m_sideInfoBuffer.ReleaseStream ();
    }

    bool InitBuffers ();

    HRESULT DecodeFrames ();
    void DecodeBadCRC (BadCRCVector &vOut);
    void DecodeSpaceAfterFrame (SpaceAfterFrameVector &vOut);

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
}}
#endif