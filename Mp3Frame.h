#ifndef __MP3_FRAME_H
#define __MP3_FRAME_H

#include "BitStreams.h"

#define MPG_MD_STEREO           0
#define MPG_MD_JOINT_STEREO     1
#define MPG_MD_DUAL_CHANNEL     2
#define MPG_MD_MONO             3

#define MP3_FRAME_HEADER_SIZE 4

namespace NCompress {
namespace NMp3 {

//
// Frame header stuff
//
enum HeaderFields
{
    mpegVersion, // 0 - MPEG 2.0, 2 - MPEG 2, 3 - MPEG 1, 2 bits
	layer, // 2 bits
	protectionBit,
	bitrateIndex, // 4 bits
	frequency, // 2 bits
	paddingBit,
	privateBit,
	channelMode, // 0 - Stereo, 1 - Join Stereo, 2 - Dual Channel, 11 - Single Chanel, 2 bits
	modeExt, // Only usable in join stereo. 2 bits
	copyrightBit,
	originalBit,
    emphasis, // 2 bits
	totalHeaderFields
};

class FrameHeader
{
    bool ComputeData ();

public:
	FrameHeader () { m_fields.SetCapacity (totalHeaderFields); }

    bool InitFromMem (Byte *header);
	bool InitFromByteBuf (const CByteBuffer & buf);

	CByteBuffer m_fields;
    
    // computed values
    UInt32 m_frameSize;
    Byte   m_sideInfoSize;
    bool   m_protection;
	UInt32 m_protectedBits;

    Byte m_data[MP3_FRAME_HEADER_SIZE];
};

class FrameHeaderCollector
{
    bool m_isEmpty;

    CByteBuffer m_fields0;
    CByteVector m_fields[totalHeaderFields];
    bool m_flags[totalHeaderFields];

    CBitWriterByteVector m_writers[totalHeaderFields];
    CBitReaderByteVector m_readers[totalHeaderFields];

public:
    FrameHeaderCollector ();

    void CollectData (FrameHeader & header);

    void ReadData (CInBuffer & buf, UInt32 numFrames);
    void WriteData (COutBuffer & buf, UInt32 numFrames);

    bool GetNextHeader (FrameHeader & headerOut);

	//void GetSideInfoSizes (UInt32 numFrames, CByteVector & sizes);
};

//
// Side Information stuff
//

class SideInfoCollector
{
	enum { MAX_SI_SIZE = 32 };

	CByteVector m_bytes[MAX_SI_SIZE];

public:
	void CollectData (Byte *data, Byte siSize);
	void GetDataForFrame (Byte *dataOut, UInt32 frameNo, Byte siSize);

    void WriteData (COutBuffer &buf);
	void ReadData (CInBuffer &buf);
};
}}
#endif