#include "stdafx.h"
#include "Mp3Utils.h"

void TransferBytes (CInBuffer *from, COutBuffer *to, UInt32 count)
{
    static Byte buf[1024 * 16];
    static const UInt32 kBufSize = sizeof (buf) / sizeof (buf[0]);

    while (from && count > 0) {
        UInt32 read = from->ReadBytes (buf, count >= kBufSize ? kBufSize : count);
        if (read == 0) return;
        if (to) to->WriteBytes (buf, read);
        count -= read;
    }
}

// CRC16 check
UInt16 CalcCRC16 (Byte *lastTwoHeaderBytes, Byte *sideInfo, UInt32 dwBitSize)
{
	UInt32 n;
	UInt16 tmpchar (0), crcmask (0), tmpi, crc = 0xffff;

	for (n = 0;  n < dwBitSize;  n++) {
		if (n % 8 == 0) {
			crcmask = 1 << 8;
            tmpchar = n == 0 ? lastTwoHeaderBytes[0] : (n == 8 ? lastTwoHeaderBytes[1] : sideInfo[(n >> 3) - 2]);
		}
		crcmask >>= 1;
		tmpi = crc & 0x8000;
		crc <<= 1;
			
		if (!tmpi ^ !(tmpchar & crcmask))
		   crc ^= 0x8005;
	}
	crc &= 0xffff;	// invert the result
	return crc;
}

void WriteUInt32 (UInt32 val, COutBuffer &buf)
{
    for (int i = 0; i < 4; i++) {
        buf.WriteByte ((Byte)val);
        val >>= 8;
    }
}

UInt32 ReadUInt32 (CInBuffer & buf)
{
    UInt32 res = 0;
    for (int i = 0; i < 4; i++)
        res |= (buf.ReadByte () << (i * 8));
    
    return res;
}
