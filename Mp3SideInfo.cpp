#include "stdafx.h"
#include "Mp3Utils.h"
#include "Mp3Frame.h"

namespace NCompress {
namespace NMp3 {

void SideInfoCollector::CollectData (Byte *data, Byte siSize)
{
	for (UInt32 i = 0; i < siSize; i++)
		m_bytes[i].Add (data[i]);
    for (UInt32 i = siSize; i < MAX_SI_SIZE; i++)
        m_bytes[i].Add (0);
}

void SideInfoCollector::GetDataForFrame (Byte *dataOut, UInt32 frameNo, Byte siSize)
{
	for (UInt32 i = 0; i < siSize; i++)
		dataOut[i] = m_bytes[i][frameNo];
}

void SideInfoCollector::WriteData (COutBuffer &buf)
{
    for (UInt32 i = 0; i < MAX_SI_SIZE; i++)
        WriteUInt32 ((UInt32)m_bytes[i].Size (), buf);

    for (UInt32 i = 0; i < MAX_SI_SIZE; i++)
        buf.WriteBytes (&m_bytes[i][0], m_bytes[i].Size ());
}

void SideInfoCollector::ReadData (CInBuffer &buf)
{
    UInt32 sizes[MAX_SI_SIZE];

    for (UInt32 i = 0; i < MAX_SI_SIZE; i++) {
        sizes[i] = ReadUInt32 (buf);
        m_bytes[i].Reserve (sizes[i]);
    }

	for (UInt32 i = 0; i < MAX_SI_SIZE; i++)
		for (UInt32 j = 0; j < sizes[i]; j++)
		    m_bytes[i].Add (buf.ReadByte ());
}   
}}