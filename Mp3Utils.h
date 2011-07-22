#ifndef __MP3_UTILS_H
#define __MP3_UTILS_H

#include "7zip/Common/InBuffer.h"
#include "7zip/Common/OutBuffer.h"

void TransferBytes (CInBuffer *from, COutBuffer *to, UInt32 count);

UInt16 CalcCRC16 (Byte *lastTwoHeaderBytes, Byte* sideInfo, UInt32 dwBitSize);

void WriteUInt32 (UInt32 val, COutBuffer &buf);
UInt32 ReadUInt32 (CInBuffer & buf);

#endif