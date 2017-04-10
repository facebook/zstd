#ifndef SEEKABLE_H
#define SEEKABLE_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "zstd_internal.h"

static const unsigned ZSTD_seekTableFooterSize = 9;

#define ZSTD_SEEKABLE_MAGICNUMBER 0x8F92EAB1

#define ZSTD_SEEKABLE_MAXCHUNKS 0x8000000U

/* 0xFE03F607 is the largest number x such that ZSTD_compressBound(x) fits in a 32-bit integer */
#define ZSTD_SEEKABLE_MAX_CHUNK_DECOMPRESSED_SIZE 0xFE03F607

#if defined (__cplusplus)
}
#endif

#endif
