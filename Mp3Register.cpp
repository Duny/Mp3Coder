#include "StdAfx.h"
#include "Mp3Decoder.h"
#include "CPP/7zip/Common/RegisterCodec.h"

static void *CreateCodec() { return (void *)(ICompressCoder2 *)(new NCompress::NMp3::CDecoder); }
#ifndef EXTRACT_ONLY
#include "Mp3Encoder.h"
static void *CreateCodecOut() { return (void *)(ICompressCoder2 *)(new NCompress::NMp3::CEncoder);  }
#else
#define CreateCodecOut 0
#endif

static CCodecInfo g_CodecInfo =
  { CreateCodec, CreateCodecOut, 0x3F44756E792E0001, L"MP3", 3, false };

REGISTER_CODEC(MP3)