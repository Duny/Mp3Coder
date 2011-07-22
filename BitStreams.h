#ifndef __BIT_STREAMS_H
#define __BIT_STREAMS_H

#include "Common/MyVector.h"
#include "Common/Buffer.h"

//==================================
//          Bit reader
//==================================
class CBitReaderMem
{
	Byte *m_ptr, m_curByte;
	Byte m_remain;
    UInt32 m_totalBits;
	
	virtual Byte GetNextByte () { 
		Byte res = 0;
		if (m_ptr) res = *m_ptr++; 
		return res;
	}

public:
	void Init (Byte *buf) { m_ptr = buf; m_remain = 8; m_totalBits = 0; m_curByte = GetNextByte (); }
    
    UInt32 GetProcessedSize () const { return m_totalBits; }

    Byte ReadBits (Byte numBits) {
        Byte res = 0;

        m_totalBits += numBits;

        while (numBits > 0) {
            Byte processed = numBits > m_remain ? m_remain : numBits;

            Byte mask = (1 << processed) - 1;

            res <<= processed;
            res |= (m_curByte >> (8 - processed)) & mask;

            m_curByte <<= processed;

            m_remain -= processed;
            if (m_remain == 0) { 
                m_curByte = GetNextByte ();
                m_remain = 8;
            }

            numBits -= processed;
        }
        return res;
    }
};

class CBitReaderByteVector : public CBitReaderMem
{
    CByteVector *m_buf;
    int m_index;

	virtual Byte GetNextByte () {
		Byte res = 0;
        if (m_buf && m_index < m_buf->Size ()) {
            res = (*m_buf)[m_index];
            m_index++;
        }
		return res;
	}

public:
	void Init (CByteVector *buf) { m_buf = buf; m_index = 0; CBitReaderMem::Init (NULL); }
};

//==================================
//          Bit writer
//==================================

class CBitWriterMem
{
	Byte m_byte, m_remain;
	Byte *m_ptr;
    UInt32 m_totalBits;
	
	virtual void WriteByte (Byte b) {
		if (m_ptr) {
			*m_ptr = m_byte;
			m_ptr++;
		}
	}

public:
	void Init (Byte *buf) { m_ptr = buf; m_remain = 8; m_byte = 0; m_totalBits = 0; }

    UInt32 GetProcessedSize () const { return m_totalBits; }

    void Flush () { if (m_remain != 8) WriteByte (m_byte); }

    void WriteBits (Byte val, Byte numBits) {
        m_totalBits += numBits;

        while (numBits > 0) {
            Byte processed = numBits > m_remain ? m_remain : numBits;

            Byte mask = ~((1 << (numBits - processed)) - 1);

            Byte tmp = (val & mask) >> (numBits - processed);

            m_byte |= (tmp << (m_remain -= processed));

            if (m_remain == 0) {
                WriteByte (m_byte);
                m_byte = 0;
                m_remain = 8;
            }
            numBits -= processed;
            val &= ~mask;
        }   
    }
};

class CBitWriterOutBuffer : public CBitWriterMem
{
    COutBuffer *m_buf;

	virtual void WriteByte (Byte b) {
		if (m_buf) m_buf->WriteByte (b);
	}

public:
	void Init (COutBuffer *buf) { m_buf = buf; CBitWriterMem::Init (NULL); };
};

class CBitWriterByteVector : public CBitWriterMem
{
    CByteVector *m_buf;

	virtual void WriteByte (Byte b) {
		if (m_buf) m_buf->Add (b);
	}

public:
	void Init (CByteVector *buf) { m_buf = buf; CBitWriterMem::Init (NULL); };
};
#endif
