/* ******************************************************************
   zstd_v08.c
   Decompression module for ZSTD v0.8 legacy format
   Copyright (C) 2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - Homepage : http://www.zstd.net/
****************************************************************** */

/*- Dependencies -*/
#include <stddef.h>     /* size_t, ptrdiff_t */
#include <string.h>     /* memcpy */
#include <stdlib.h>     /* malloc, free, qsort */

#define XXH_STATIC_LINKING_ONLY   /* XXH64_state_t */
#include "xxhash.h"      /* XXH64_* */
#include "zstd_v08.h"

#define FSEv08_STATIC_LINKING_ONLY  /* FSEv08_MIN_TABLELOG */
#define HUFv08_STATIC_LINKING_ONLY  /* HUFv08_TABLELOG_ABSOLUTEMAX */
#define ZSTDv08_STATIC_LINKING_ONLY 



#ifdef ZSTDv08_STATIC_LINKING_ONLY

/* ====================================================================================
 * The definitions in this section are considered experimental.
 * They should never be used with a dynamic library, as they may change in the future.
 * They are provided for advanced usages.
 * Use them only in association with static linking.
 * ==================================================================================== */

/*--- Constants ---*/
#define ZSTDv08_MAGIC_SKIPPABLE_START  0x184D2A50U

#define ZSTDv08_WINDOWLOG_MAX_32  25
#define ZSTDv08_WINDOWLOG_MAX_64  27
#define ZSTDv08_WINDOWLOG_MAX    ((U32)(MEM_32bits() ? ZSTDv08_WINDOWLOG_MAX_32 : ZSTDv08_WINDOWLOG_MAX_64))
#define ZSTDv08_WINDOWLOG_MIN     18
#define ZSTDv08_CHAINLOG_MAX     (ZSTDv08_WINDOWLOG_MAX+1)
#define ZSTDv08_CHAINLOG_MIN       4
#define ZSTDv08_HASHLOG_MAX       ZSTDv08_WINDOWLOG_MAX
#define ZSTDv08_HASHLOG_MIN       12
#define ZSTDv08_HASHLOG3_MAX      17
#define ZSTDv08_SEARCHLOG_MAX    (ZSTDv08_WINDOWLOG_MAX-1)
#define ZSTDv08_SEARCHLOG_MIN      1
#define ZSTDv08_SEARCHLENGTH_MAX   7
#define ZSTDv08_SEARCHLENGTH_MIN   3
#define ZSTDv08_TARGETLENGTH_MIN   4
#define ZSTDv08_TARGETLENGTH_MAX 999

#define ZSTDv08_FRAMEHEADERSIZE_MAX 18    /* for static allocation */
static const size_t ZSTDv08_frameHeaderSize_min = 5;
static const size_t ZSTDv08_frameHeaderSize_max = ZSTDv08_FRAMEHEADERSIZE_MAX;
static const size_t ZSTDv08_skippableHeaderSize = 8;  /* magic number + skippable frame length */


/* custom memory allocation functions */
typedef void* (*ZSTDv08_allocFunction) (void* opaque, size_t size);
typedef void  (*ZSTDv08_freeFunction) (void* opaque, void* address);
typedef struct { ZSTDv08_allocFunction customAlloc; ZSTDv08_freeFunction customFree; void* opaque; } ZSTDv08_customMem;


/*--- Advanced Decompression functions ---*/

/*! ZSTDv08_estimateDCtxSize() :
 *  Gives the potential amount of memory allocated to create a ZSTDv08_DCtx */
ZSTDLIB_API size_t ZSTDv08_estimateDCtxSize(void);

/*! ZSTDv08_createDCtx_advanced() :
 *  Create a ZSTD decompression context using external alloc and free functions */
ZSTDLIB_API ZSTDv08_DCtx* ZSTDv08_createDCtx_advanced(ZSTDv08_customMem customMem);

/*! ZSTDv08_sizeofDCtx() :
 *  Gives the amount of memory used by a given ZSTDv08_DCtx */
ZSTDLIB_API size_t ZSTDv08_sizeofDCtx(const ZSTDv08_DCtx* dctx);


/* ******************************************************************
*  Buffer-less streaming functions (synchronous mode)
********************************************************************/
/* This is an advanced API, giving full control over buffer management, for users which need direct control over memory.
*  But it's also a complex one, with a lot of restrictions (documented below).
*  For an easier streaming API, look into common/zbuff.h
*  which removes all restrictions by allocating and managing its own internal buffer */

ZSTDLIB_API size_t ZSTDv08_decompressBegin(ZSTDv08_DCtx* dctx);
ZSTDLIB_API size_t ZSTDv08_decompressBegin_usingDict(ZSTDv08_DCtx* dctx, const void* dict, size_t dictSize);
ZSTDLIB_API void   ZSTDv08_copyDCtx(ZSTDv08_DCtx* dctx, const ZSTDv08_DCtx* preparedDCtx);

ZSTDLIB_API size_t ZSTDv08_nextSrcSizeToDecompress(ZSTDv08_DCtx* dctx);
ZSTDLIB_API size_t ZSTDv08_decompressContinue(ZSTDv08_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);

typedef enum { ZSTDnit_frameHeader, ZSTDnit_blockHeader, ZSTDnit_block, ZSTDnit_lastBlock, ZSTDnit_checksum, ZSTDnit_skippableFrame } ZSTDv08_nextInputType_e;
ZSTDLIB_API ZSTDv08_nextInputType_e ZSTDv08_nextInputType(ZSTDv08_DCtx* dctx);

/*
  Buffer-less streaming decompression (synchronous mode)

  A ZSTDv08_DCtx object is required to track streaming operations.
  Use ZSTDv08_createDCtx() / ZSTDv08_freeDCtx() to manage it.
  A ZSTDv08_DCtx object can be re-used multiple times.

  First typical operation is to retrieve frame parameters, using ZSTDv08_getFrameParams().
  It fills a ZSTDv08_frameParams structure which provide important information to correctly decode the frame,
  such as the minimum rolling buffer size to allocate to decompress data (`windowSize`),
  and the dictionary ID used.
  (Note : content size is optional, it may not be present. 0 means : content size unknown).
  Note that these values could be wrong, either because of data malformation, or because an attacker is spoofing deliberate false information.
  As a consequence, check that values remain within valid application range, especially `windowSize`, before allocation.
  Each application can set its own limit, depending on local restrictions. For extended interoperability, it is recommended to support at least 8 MB.
  Frame parameters are extracted from the beginning of the compressed frame.
  Data fragment must be large enough to ensure successful decoding, typically `ZSTDv08_frameHeaderSize_max` bytes.
  @result : 0 : successful decoding, the `ZSTDv08_frameParams` structure is correctly filled.
           >0 : `srcSize` is too small, please provide at least @result bytes on next attempt.
           errorCode, which can be tested using ZSTDv08_isError().

  Start decompression, with ZSTDv08_decompressBegin() or ZSTDv08_decompressBegin_usingDict().
  Alternatively, you can copy a prepared context, using ZSTDv08_copyDCtx().

  Then use ZSTDv08_nextSrcSizeToDecompress() and ZSTDv08_decompressContinue() alternatively.
  ZSTDv08_nextSrcSizeToDecompress() tells how many bytes to provide as 'srcSize' to ZSTDv08_decompressContinue().
  ZSTDv08_decompressContinue() requires this _exact_ amount of bytes, or it will fail.

  @result of ZSTDv08_decompressContinue() is the number of bytes regenerated within 'dst' (necessarily <= dstCapacity).
  It can be zero, which is not an error; it just means ZSTDv08_decompressContinue() has decoded some metadata item.
  It can also be an error code, which can be tested with ZSTDv08_isError().

  ZSTDv08_decompressContinue() needs previous data blocks during decompression, up to `windowSize`.
  They should preferably be located contiguously, prior to current block.
  Alternatively, a round buffer of sufficient size is also possible. Sufficient size is determined by frame parameters.
  ZSTDv08_decompressContinue() is very sensitive to contiguity,
  if 2 blocks don't follow each other, make sure that either the compressor breaks contiguity at the same place,
  or that previous contiguous segment is large enough to properly handle maximum back-reference.

  A frame is fully decoded when ZSTDv08_nextSrcSizeToDecompress() returns zero.
  Context can then be reset to start a new decompression.

  Note : it's possible to know if next input to present is a header or a block, using ZSTDv08_nextInputType().
  This information is not required to properly decode a frame.

  == Special case : skippable frames ==

  Skippable frames allow integration of user-defined data into a flow of concatenated frames.
  Skippable frames will be ignored (skipped) by a decompressor. The format of skippable frames is as follows :
  a) Skippable frame ID - 4 Bytes, Little endian format, any value from 0x184D2A50 to 0x184D2A5F
  b) Frame Size - 4 Bytes, Little endian format, unsigned 32-bits
  c) Frame Content - any content (User Data) of length equal to Frame Size
  For skippable frames ZSTDv08_decompressContinue() always returns 0.
  For skippable frames ZSTDv08_getFrameParams() returns fparamsPtr->windowLog==0 what means that a frame is skippable.
  It also returns Frame Size as fparamsPtr->frameContentSize.
*/


/* **************************************
*  Block functions
****************************************/
/*! Block functions produce and decode raw zstd blocks, without frame metadata.
    Frame metadata cost is typically ~18 bytes, which can be non-negligible for very small blocks (< 100 bytes).
    User will have to take in charge required information to regenerate data, such as compressed and content sizes.

    A few rules to respect :
    - Compressing and decompressing require a context structure
      + Use ZSTDv08_createCCtx() and ZSTDv08_createDCtx()
    - It is necessary to init context before starting
      + compression : ZSTDv08_compressBegin()
      + decompression : ZSTDv08_decompressBegin()
      + variants _usingDict() are also allowed
      + copyCCtx() and copyDCtx() work too
    - Block size is limited, it must be <= ZSTDv08_getBlockSizeMax()
      + If you need to compress more, cut data into multiple blocks
      + Consider using the regular ZSTDv08_compress() instead, as frame metadata costs become negligible when source size is large.
    - When a block is considered not compressible enough, ZSTDv08_compressBlock() result will be zero.
      In which case, nothing is produced into `dst`.
      + User must test for such outcome and deal directly with uncompressed data
      + ZSTDv08_decompressBlock() doesn't accept uncompressed data as input !!!
      + In case of multiple successive blocks, decoder must be informed of uncompressed block existence to follow proper history.
        Use ZSTDv08_insertBlock() in such a case.
*/

#define ZSTDv08_BLOCKSIZE_ABSOLUTEMAX (128 * 1024)   /* define, for static allocation */
ZSTDLIB_API size_t ZSTDv08_decompressBlock(ZSTDv08_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);
ZSTDLIB_API size_t ZSTDv08_insertBlock(ZSTDv08_DCtx* dctx, const void* blockStart, size_t blockSize);  /**< insert block into `dctx` history. Useful for uncompressed blocks */


#endif   /* ZSTDv08_STATIC_LINKING_ONLY */


/* ====================================================================================
 * The definitions in this section are considered experimental.
 * They should never be used in association with a dynamic library, as they may change in the future.
 * They are provided for advanced usages.
 * Use them only in association with static linking.
 * ==================================================================================== */

/*! ZBUFFv08_createDCtx_advanced() :
 *  Create a ZBUFF decompression context using external alloc and free functions */
ZSTDLIB_API ZBUFFv08_DCtx* ZBUFFv08_createDCtx_advanced(ZSTDv08_customMem customMem);



/* ******************************************************************
   mem.h
   low-level memory access routines
   Copyright (C) 2013-2015, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef MEM_H_MODULE
#define MEM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif



/*-****************************************
*  Compiler specifics
******************************************/
#if defined(_MSC_VER)   /* Visual Studio */
#   include <stdlib.h>  /* _byteswap_ulong */
#   include <intrin.h>  /* _byteswap_* */
#endif
#if defined(__GNUC__)
#  define MEM_STATIC static __inline __attribute__((unused))
#elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#  define MEM_STATIC static inline
#elif defined(_MSC_VER)
#  define MEM_STATIC static __inline
#else
#  define MEM_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif

/* code only tested on 32 and 64 bits systems */
#define MEM_STATIC_ASSERT(c)   { enum { XXH_static_assert = 1/(int)(!!(c)) }; }
MEM_STATIC void MEM_check(void) { MEM_STATIC_ASSERT((sizeof(size_t)==4) || (sizeof(size_t)==8)); }


/*-**************************************************************
*  Basic Types
*****************************************************************/
#if  !defined (__VMS) && (defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
# include <stdint.h>
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef  int16_t S16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
  typedef  int64_t S64;
#else
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef   signed short      S16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
  typedef   signed long long  S64;
#endif


/*-**************************************************************
*  Memory I/O
*****************************************************************/
/* MEM_FORCE_MEMORY_ACCESS :
 * By default, access to unaligned memory is controlled by `memcpy()`, which is safe and portable.
 * Unfortunately, on some target/compiler combinations, the generated assembly is sub-optimal.
 * The below switch allow to select different access method for improved performance.
 * Method 0 (default) : use `memcpy()`. Safe and portable.
 * Method 1 : `__packed` statement. It depends on compiler extension (ie, not portable).
 *            This method is safe if your compiler supports it, and *generally* as fast or faster than `memcpy`.
 * Method 2 : direct access. This method is portable but violate C standard.
 *            It can generate buggy code on targets depending on alignment.
 *            In some circumstances, it's the only known way to get the most performance (ie GCC + ARMv6)
 * See http://fastcompression.blogspot.fr/2015/08/accessing-unaligned-memory.html for details.
 * Prefer these methods in priority order (0 > 1 > 2)
 */
#ifndef MEM_FORCE_MEMORY_ACCESS   /* can be defined externally, on command line for example */
#  if defined(__GNUC__) && ( defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) )
#    define MEM_FORCE_MEMORY_ACCESS 2
#  elif defined(__INTEL_COMPILER) || \
  (defined(__GNUC__) && ( defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) ))
#    define MEM_FORCE_MEMORY_ACCESS 1
#  endif
#endif

MEM_STATIC unsigned MEM_32bits(void) { return sizeof(size_t)==4; }
MEM_STATIC unsigned MEM_64bits(void) { return sizeof(size_t)==8; }

MEM_STATIC unsigned MEM_isLittleEndian(void)
{
    const union { U32 u; BYTE c[4]; } one = { 1 };   /* don't use static : performance detrimental  */
    return one.c[0];
}

#if defined(MEM_FORCE_MEMORY_ACCESS) && (MEM_FORCE_MEMORY_ACCESS==2)

/* violates C standard, by lying on structure alignment.
Only use if no other choice to achieve best performance on target platform */
MEM_STATIC U16 MEM_read16(const void* memPtr) { return *(const U16*) memPtr; }
MEM_STATIC U32 MEM_read32(const void* memPtr) { return *(const U32*) memPtr; }
MEM_STATIC U64 MEM_read64(const void* memPtr) { return *(const U64*) memPtr; }
MEM_STATIC U64 MEM_readST(const void* memPtr) { return *(const size_t*) memPtr; }

MEM_STATIC void MEM_write16(void* memPtr, U16 value) { *(U16*)memPtr = value; }
MEM_STATIC void MEM_write32(void* memPtr, U32 value) { *(U32*)memPtr = value; }
MEM_STATIC void MEM_write64(void* memPtr, U64 value) { *(U64*)memPtr = value; }

#elif defined(MEM_FORCE_MEMORY_ACCESS) && (MEM_FORCE_MEMORY_ACCESS==1)

/* __pack instructions are safer, but compiler specific, hence potentially problematic for some compilers */
/* currently only defined for gcc and icc */
typedef union { U16 u16; U32 u32; U64 u64; size_t st; } __attribute__((packed)) unalign;

MEM_STATIC U16 MEM_read16(const void* ptr) { return ((const unalign*)ptr)->u16; }
MEM_STATIC U32 MEM_read32(const void* ptr) { return ((const unalign*)ptr)->u32; }
MEM_STATIC U64 MEM_read64(const void* ptr) { return ((const unalign*)ptr)->u64; }
MEM_STATIC U64 MEM_readST(const void* ptr) { return ((const unalign*)ptr)->st; }

MEM_STATIC void MEM_write16(void* memPtr, U16 value) { ((unalign*)memPtr)->u16 = value; }
MEM_STATIC void MEM_write32(void* memPtr, U32 value) { ((unalign*)memPtr)->u32 = value; }
MEM_STATIC void MEM_write64(void* memPtr, U64 value) { ((unalign*)memPtr)->u64 = value; }

#else

/* default method, safe and standard.
   can sometimes prove slower */

MEM_STATIC U16 MEM_read16(const void* memPtr)
{
    U16 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC U32 MEM_read32(const void* memPtr)
{
    U32 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC U64 MEM_read64(const void* memPtr)
{
    U64 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC size_t MEM_readST(const void* memPtr)
{
    size_t val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC void MEM_write16(void* memPtr, U16 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

MEM_STATIC void MEM_write32(void* memPtr, U32 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

MEM_STATIC void MEM_write64(void* memPtr, U64 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

#endif /* MEM_FORCE_MEMORY_ACCESS */

MEM_STATIC U32 MEM_swap32(U32 in)
{
#if defined(_MSC_VER)     /* Visual Studio */
    return _byteswap_ulong(in);
#elif defined (__GNUC__)
    return __builtin_bswap32(in);
#else
    return  ((in << 24) & 0xff000000 ) |
            ((in <<  8) & 0x00ff0000 ) |
            ((in >>  8) & 0x0000ff00 ) |
            ((in >> 24) & 0x000000ff );
#endif
}

MEM_STATIC U64 MEM_swap64(U64 in)
{
#if defined(_MSC_VER)     /* Visual Studio */
    return _byteswap_uint64(in);
#elif defined (__GNUC__)
    return __builtin_bswap64(in);
#else
    return  ((in << 56) & 0xff00000000000000ULL) |
            ((in << 40) & 0x00ff000000000000ULL) |
            ((in << 24) & 0x0000ff0000000000ULL) |
            ((in << 8)  & 0x000000ff00000000ULL) |
            ((in >> 8)  & 0x00000000ff000000ULL) |
            ((in >> 24) & 0x0000000000ff0000ULL) |
            ((in >> 40) & 0x000000000000ff00ULL) |
            ((in >> 56) & 0x00000000000000ffULL);
#endif
}

MEM_STATIC size_t MEM_swapST(size_t in)
{
    if (MEM_32bits())
        return (size_t)MEM_swap32((U32)in);
    else
        return (size_t)MEM_swap64((U64)in);
}

/*=== Little endian r/w ===*/

MEM_STATIC U16 MEM_readLE16(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read16(memPtr);
    else {
        const BYTE* p = (const BYTE*)memPtr;
        return (U16)(p[0] + (p[1]<<8));
    }
}

MEM_STATIC void MEM_writeLE16(void* memPtr, U16 val)
{
    if (MEM_isLittleEndian()) {
        MEM_write16(memPtr, val);
    } else {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE)val;
        p[1] = (BYTE)(val>>8);
    }
}

MEM_STATIC U32 MEM_readLE24(const void* memPtr)
{
    return MEM_readLE16(memPtr) + (((const BYTE*)memPtr)[2] << 16);
}

MEM_STATIC void MEM_writeLE24(void* memPtr, U32 val)
{
    MEM_writeLE16(memPtr, (U16)val);
    ((BYTE*)memPtr)[2] = (BYTE)(val>>16);
}

MEM_STATIC U32 MEM_readLE32(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read32(memPtr);
    else
        return MEM_swap32(MEM_read32(memPtr));
}

MEM_STATIC void MEM_writeLE32(void* memPtr, U32 val32)
{
    if (MEM_isLittleEndian())
        MEM_write32(memPtr, val32);
    else
        MEM_write32(memPtr, MEM_swap32(val32));
}

MEM_STATIC U64 MEM_readLE64(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read64(memPtr);
    else
        return MEM_swap64(MEM_read64(memPtr));
}

MEM_STATIC void MEM_writeLE64(void* memPtr, U64 val64)
{
    if (MEM_isLittleEndian())
        MEM_write64(memPtr, val64);
    else
        MEM_write64(memPtr, MEM_swap64(val64));
}

MEM_STATIC size_t MEM_readLEST(const void* memPtr)
{
    if (MEM_32bits())
        return (size_t)MEM_readLE32(memPtr);
    else
        return (size_t)MEM_readLE64(memPtr);
}

MEM_STATIC void MEM_writeLEST(void* memPtr, size_t val)
{
    if (MEM_32bits())
        MEM_writeLE32(memPtr, (U32)val);
    else
        MEM_writeLE64(memPtr, (U64)val);
}

/*=== Big endian r/w ===*/

MEM_STATIC U32 MEM_readBE32(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_swap32(MEM_read32(memPtr));
    else
        return MEM_read32(memPtr);
}

MEM_STATIC void MEM_writeBE32(void* memPtr, U32 val32)
{
    if (MEM_isLittleEndian())
        MEM_write32(memPtr, MEM_swap32(val32));
    else
        MEM_write32(memPtr, val32);
}

MEM_STATIC U64 MEM_readBE64(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_swap64(MEM_read64(memPtr));
    else
        return MEM_read64(memPtr);
}

MEM_STATIC void MEM_writeBE64(void* memPtr, U64 val64)
{
    if (MEM_isLittleEndian())
        MEM_write64(memPtr, MEM_swap64(val64));
    else
        MEM_write64(memPtr, val64);
}

MEM_STATIC size_t MEM_readBEST(const void* memPtr)
{
    if (MEM_32bits())
        return (size_t)MEM_readBE32(memPtr);
    else
        return (size_t)MEM_readBE64(memPtr);
}

MEM_STATIC void MEM_writeBEST(void* memPtr, size_t val)
{
    if (MEM_32bits())
        MEM_writeBE32(memPtr, (U32)val);
    else
        MEM_writeBE64(memPtr, (U64)val);
}


/* function safe only for comparisons */
MEM_STATIC U32 MEM_readMINMATCH(const void* memPtr, U32 length)
{
    switch (length)
    {
    default :
    case 4 : return MEM_read32(memPtr);
    case 3 : if (MEM_isLittleEndian())
                return MEM_read32(memPtr)<<8;
             else
                return MEM_read32(memPtr)>>8;
    }
}

#if defined (__cplusplus)
}
#endif

#endif /* MEM_H_MODULE */
/* ******************************************************************
   Error codes list
   Copyright (C) 2016, Yann Collet

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - Homepage : http://www.zstd.net
****************************************************************** */
#ifndef ERROR_PUBLIC_H_MODULE
#define ERROR_PUBLIC_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif


/* ****************************************
*  error codes list
******************************************/
typedef enum {
  ZSTDv08_error_no_error,
  ZSTDv08_error_GENERIC,
  ZSTDv08_error_prefix_unknown,
  ZSTDv08_error_frameParameter_unsupported,
  ZSTDv08_error_frameParameter_unsupportedBy32bits,
  ZSTDv08_error_compressionParameter_unsupported,
  ZSTDv08_error_init_missing,
  ZSTDv08_error_memory_allocation,
  ZSTDv08_error_stage_wrong,
  ZSTDv08_error_dstSize_tooSmall,
  ZSTDv08_error_srcSize_wrong,
  ZSTDv08_error_corruption_detected,
  ZSTDv08_error_checksum_wrong,
  ZSTDv08_error_tableLog_tooLarge,
  ZSTDv08_error_maxSymbolValue_tooLarge,
  ZSTDv08_error_maxSymbolValue_tooSmall,
  ZSTDv08_error_dictionary_corrupted,
  ZSTDv08_error_dictionary_wrong,
  ZSTDv08_error_maxCode
} ZSTDv08_ErrorCode;

/*! ZSTDv08_getErrorCode() :
    convert a `size_t` function result into a `ZSTDv08_ErrorCode` enum type,
    which can be used to compare directly with enum list published into "error_public.h" */
ZSTDv08_ErrorCode ZSTDv08_getErrorCode(size_t functionResult);
const char* ZSTDv08_getErrorString(ZSTDv08_ErrorCode code);


#if defined (__cplusplus)
}
#endif

#endif /* ERROR_PUBLIC_H_MODULE */
/* ******************************************************************
   Error codes and messages
   Copyright (C) 2013-2016, Yann Collet

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - Homepage : http://www.zstd.net
****************************************************************** */
/* Note : this module is expected to remain private, do not expose it */

#ifndef ERROR_H_MODULE
#define ERROR_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif



/* ****************************************
*  Compiler-specific
******************************************/
#if defined(__GNUC__)
#  define ERR_STATIC static __attribute__((unused))
#elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#  define ERR_STATIC static inline
#elif defined(_MSC_VER)
#  define ERR_STATIC static __inline
#else
#  define ERR_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif


/*-****************************************
*  Customization (error_public.h)
******************************************/
typedef ZSTDv08_ErrorCode ERR_enum;
#define PREFIX(name) ZSTDv08_error_##name


/*-****************************************
*  Error codes handling
******************************************/
#ifdef ERROR
#  undef ERROR   /* reported already defined on VS 2015 (Rich Geldreich) */
#endif
#define ERROR(name) ((size_t)-PREFIX(name))

ERR_STATIC unsigned ERR_isError(size_t code) { return (code > ERROR(maxCode)); }

ERR_STATIC ERR_enum ERR_getErrorCode(size_t code) { if (!ERR_isError(code)) return (ERR_enum)0; return (ERR_enum) (0-code); }


/*-****************************************
*  Error Strings
******************************************/

ERR_STATIC const char* ERR_getErrorString(ERR_enum code)
{
    static const char* notErrorCode = "Unspecified error code";
    switch( code )
    {
    case PREFIX(no_error): return "No error detected";
    case PREFIX(GENERIC):  return "Error (generic)";
    case PREFIX(prefix_unknown): return "Unknown frame descriptor";
    case PREFIX(frameParameter_unsupported): return "Unsupported frame parameter";
    case PREFIX(frameParameter_unsupportedBy32bits): return "Frame parameter unsupported in 32-bits mode";
    case PREFIX(compressionParameter_unsupported): return "Compression parameter is out of bound";
    case PREFIX(init_missing): return "Context should be init first";
    case PREFIX(memory_allocation): return "Allocation error : not enough memory";
    case PREFIX(stage_wrong): return "Operation not authorized at current processing stage";
    case PREFIX(dstSize_tooSmall): return "Destination buffer is too small";
    case PREFIX(srcSize_wrong): return "Src size incorrect";
    case PREFIX(corruption_detected): return "Corrupted block detected";
    case PREFIX(checksum_wrong): return "Restored data doesn't match checksum";
    case PREFIX(tableLog_tooLarge): return "tableLog requires too much memory : unsupported";
    case PREFIX(maxSymbolValue_tooLarge): return "Unsupported max Symbol Value : too large";
    case PREFIX(maxSymbolValue_tooSmall): return "Specified maxSymbolValue is too small";
    case PREFIX(dictionary_corrupted): return "Dictionary is corrupted";
    case PREFIX(dictionary_wrong): return "Dictionary mismatch";
    case PREFIX(maxCode):
    default: return notErrorCode;
    }
}

ERR_STATIC const char* ERR_getErrorName(size_t code)
{
    return ERR_getErrorString(ERR_getErrorCode(code));
}

#if defined (__cplusplus)
}
#endif

#endif /* ERROR_H_MODULE */
/* ******************************************************************
   bitstream
   Part of FSE library
   header file (to include)
   Copyright (C) 2013-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
****************************************************************** */
#ifndef BITSTREAM_H_MODULE
#define BITSTREAM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif


/*
*  This API consists of small unitary functions, which must be inlined for best performance.
*  Since link-time-optimization is not available for all compilers,
*  these functions are defined into a .h to be included.
*/


/*=========================================
*  Target specific
=========================================*/
#if defined(__BMI__) && defined(__GNUC__)
#  include <immintrin.h>   /* support for bextr (experimental) */
#endif


/*-********************************************
*  bitStream decoding API (read backward)
**********************************************/
typedef struct
{
    size_t   bitContainer;
    unsigned bitsConsumed;
    const char* ptr;
    const char* start;
} BIT_DStream_t;

typedef enum { BIT_DStream_unfinished = 0,
               BIT_DStream_endOfBuffer = 1,
               BIT_DStream_completed = 2,
               BIT_DStream_overflow = 3 } BIT_DStream_status;  /* result of BIT_reloadDStream() */
               /* 1,2,4,8 would be better for bitmap combinations, but slows down performance a bit ... :( */

MEM_STATIC size_t   BIT_initDStream(BIT_DStream_t* bitD, const void* srcBuffer, size_t srcSize);
MEM_STATIC size_t   BIT_readBits(BIT_DStream_t* bitD, unsigned nbBits);
MEM_STATIC BIT_DStream_status BIT_reloadDStream(BIT_DStream_t* bitD);
MEM_STATIC unsigned BIT_endOfDStream(const BIT_DStream_t* bitD);


/* Start by invoking BIT_initDStream().
*  A chunk of the bitStream is then stored into a local register.
*  Local register size is 64-bits on 64-bits systems, 32-bits on 32-bits systems (size_t).
*  You can then retrieve bitFields stored into the local register, **in reverse order**.
*  Local register is explicitly reloaded from memory by the BIT_reloadDStream() method.
*  A reload guarantee a minimum of ((8*sizeof(bitD->bitContainer))-7) bits when its result is BIT_DStream_unfinished.
*  Otherwise, it can be less than that, so proceed accordingly.
*  Checking if DStream has reached its end can be performed with BIT_endOfDStream().
*/


/*-****************************************
*  unsafe API
******************************************/
MEM_STATIC size_t BIT_readBitsFast(BIT_DStream_t* bitD, unsigned nbBits);
/* faster, but works only if nbBits >= 1 */



/*-**************************************************************
*  Internal functions
****************************************************************/
MEM_STATIC unsigned BIT_highbit32 (register U32 val)
{
#   if defined(_MSC_VER)   /* Visual */
    unsigned long r=0;
    _BitScanReverse ( &r, val );
    return (unsigned) r;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   /* Use GCC Intrinsic */
    return 31 - __builtin_clz (val);
#   else   /* Software version */
    static const unsigned DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
    U32 v = val;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return DeBruijnClz[ (U32) (v * 0x07C4ACDDU) >> 27];
#   endif
}

/*=====    Local Constants   =====*/
static const unsigned BIT_mask[] = { 0, 1, 3, 7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF, 0x1FFFF, 0x3FFFF, 0x7FFFF, 0xFFFFF, 0x1FFFFF, 0x3FFFFF, 0x7FFFFF,  0xFFFFFF, 0x1FFFFFF, 0x3FFFFFF };   /* up to 26 bits */


/*-********************************************************
* bitStream decoding
**********************************************************/
/*! BIT_initDStream() :
*   Initialize a BIT_DStream_t.
*   `bitD` : a pointer to an already allocated BIT_DStream_t structure.
*   `srcSize` must be the *exact* size of the bitStream, in bytes.
*   @return : size of stream (== srcSize) or an errorCode if a problem is detected
*/
MEM_STATIC size_t BIT_initDStream(BIT_DStream_t* bitD, const void* srcBuffer, size_t srcSize)
{
    if (srcSize < 1) { memset(bitD, 0, sizeof(*bitD)); return ERROR(srcSize_wrong); }

    if (srcSize >=  sizeof(bitD->bitContainer)) {  /* normal case */
        bitD->start = (const char*)srcBuffer;
        bitD->ptr   = (const char*)srcBuffer + srcSize - sizeof(bitD->bitContainer);
        bitD->bitContainer = MEM_readLEST(bitD->ptr);
        { BYTE const lastByte = ((const BYTE*)srcBuffer)[srcSize-1];
          bitD->bitsConsumed = lastByte ? 8 - BIT_highbit32(lastByte) : 0;
          if (lastByte == 0) return ERROR(GENERIC); /* endMark not present */ }
    } else {
        bitD->start = (const char*)srcBuffer;
        bitD->ptr   = bitD->start;
        bitD->bitContainer = *(const BYTE*)(bitD->start);
        switch(srcSize)
        {
            case 7: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[6]) << (sizeof(bitD->bitContainer)*8 - 16);
            case 6: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[5]) << (sizeof(bitD->bitContainer)*8 - 24);
            case 5: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[4]) << (sizeof(bitD->bitContainer)*8 - 32);
            case 4: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[3]) << 24;
            case 3: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[2]) << 16;
            case 2: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[1]) <<  8;
            default:;
        }
        { BYTE const lastByte = ((const BYTE*)srcBuffer)[srcSize-1];
          bitD->bitsConsumed = lastByte ? 8 - BIT_highbit32(lastByte) : 0;
          if (lastByte == 0) return ERROR(GENERIC); /* endMark not present */ }
        bitD->bitsConsumed += (U32)(sizeof(bitD->bitContainer) - srcSize)*8;
    }

    return srcSize;
}

MEM_STATIC size_t BIT_getUpperBits(size_t bitContainer, U32 const start)
{
    return bitContainer >> start;
}

MEM_STATIC size_t BIT_getMiddleBits(size_t bitContainer, U32 const start, U32 const nbBits)
{
#if defined(__BMI__) && defined(__GNUC__)   /* experimental */
#  if defined(__x86_64__)
    if (sizeof(bitContainer)==8)
        return _bextr_u64(bitContainer, start, nbBits);
    else
#  endif
        return _bextr_u32(bitContainer, start, nbBits);
#else
    return (bitContainer >> start) & BIT_mask[nbBits];
#endif
}

MEM_STATIC size_t BIT_getLowerBits(size_t bitContainer, U32 const nbBits)
{
    return bitContainer & BIT_mask[nbBits];
}

/*! BIT_lookBits() :
 *  Provides next n bits from local register.
 *  local register is not modified.
 *  On 32-bits, maxNbBits==24.
 *  On 64-bits, maxNbBits==56.
 *  @return : value extracted
 */
 MEM_STATIC size_t BIT_lookBits(const BIT_DStream_t* bitD, U32 nbBits)
{
#if defined(__BMI__) && defined(__GNUC__)   /* experimental; fails if bitD->bitsConsumed + nbBits > sizeof(bitD->bitContainer)*8 */
    return BIT_getMiddleBits(bitD->bitContainer, (sizeof(bitD->bitContainer)*8) - bitD->bitsConsumed - nbBits, nbBits);
#else
    U32 const bitMask = sizeof(bitD->bitContainer)*8 - 1;
    return ((bitD->bitContainer << (bitD->bitsConsumed & bitMask)) >> 1) >> ((bitMask-nbBits) & bitMask);
#endif
}

/*! BIT_lookBitsFast() :
*   unsafe version; only works only if nbBits >= 1 */
MEM_STATIC size_t BIT_lookBitsFast(const BIT_DStream_t* bitD, U32 nbBits)
{
    U32 const bitMask = sizeof(bitD->bitContainer)*8 - 1;
    return (bitD->bitContainer << (bitD->bitsConsumed & bitMask)) >> (((bitMask+1)-nbBits) & bitMask);
}

MEM_STATIC void BIT_skipBits(BIT_DStream_t* bitD, U32 nbBits)
{
    bitD->bitsConsumed += nbBits;
}

/*! BIT_readBits() :
 *  Read (consume) next n bits from local register and update.
 *  Pay attention to not read more than nbBits contained into local register.
 *  @return : extracted value.
 */
MEM_STATIC size_t BIT_readBits(BIT_DStream_t* bitD, U32 nbBits)
{
    size_t const value = BIT_lookBits(bitD, nbBits);
    BIT_skipBits(bitD, nbBits);
    return value;
}

/*! BIT_readBitsFast() :
*   unsafe version; only works only if nbBits >= 1 */
MEM_STATIC size_t BIT_readBitsFast(BIT_DStream_t* bitD, U32 nbBits)
{
    size_t const value = BIT_lookBitsFast(bitD, nbBits);
    BIT_skipBits(bitD, nbBits);
    return value;
}

/*! BIT_reloadDStream() :
*   Refill `BIT_DStream_t` from src buffer previously defined (see BIT_initDStream() ).
*   This function is safe, it guarantees it will not read beyond src buffer.
*   @return : status of `BIT_DStream_t` internal register.
              if status == unfinished, internal register is filled with >= (sizeof(bitD->bitContainer)*8 - 7) bits */
MEM_STATIC BIT_DStream_status BIT_reloadDStream(BIT_DStream_t* bitD)
{
	if (bitD->bitsConsumed > (sizeof(bitD->bitContainer)*8))  /* should not happen => corruption detected */
		return BIT_DStream_overflow;

    if (bitD->ptr >= bitD->start + sizeof(bitD->bitContainer)) {
        bitD->ptr -= bitD->bitsConsumed >> 3;
        bitD->bitsConsumed &= 7;
        bitD->bitContainer = MEM_readLEST(bitD->ptr);
        return BIT_DStream_unfinished;
    }
    if (bitD->ptr == bitD->start) {
        if (bitD->bitsConsumed < sizeof(bitD->bitContainer)*8) return BIT_DStream_endOfBuffer;
        return BIT_DStream_completed;
    }
    {   U32 nbBytes = bitD->bitsConsumed >> 3;
        BIT_DStream_status result = BIT_DStream_unfinished;
        if (bitD->ptr - nbBytes < bitD->start) {
            nbBytes = (U32)(bitD->ptr - bitD->start);  /* ptr > start */
            result = BIT_DStream_endOfBuffer;
        }
        bitD->ptr -= nbBytes;
        bitD->bitsConsumed -= nbBytes*8;
        bitD->bitContainer = MEM_readLEST(bitD->ptr);   /* reminder : srcSize > sizeof(bitD) */
        return result;
    }
}

/*! BIT_endOfDStream() :
*   @return Tells if DStream has exactly reached its end (all bits consumed).
*/
MEM_STATIC unsigned BIT_endOfDStream(const BIT_DStream_t* DStream)
{
    return ((DStream->ptr == DStream->start) && (DStream->bitsConsumed == sizeof(DStream->bitContainer)*8));
}

#if defined (__cplusplus)
}
#endif

#endif /* BITSTREAM_H_MODULE */
/* ******************************************************************
   FSE : Finite State Entropy codec
   Public Prototypes declaration
   Copyright (C) 2013-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
****************************************************************** */
#ifndef FSEv08_H
#define FSEv08_H

#if defined (__cplusplus)
extern "C" {
#endif



/*-****************************************
*  FSE simple functions
******************************************/
/*! FSEv08_decompress():
    Decompress FSE data from buffer 'cSrc', of size 'cSrcSize',
    into already allocated destination buffer 'dst', of size 'dstCapacity'.
    @return : size of regenerated data (<= maxDstSize),
              or an error code, which can be tested using FSEv08_isError() .

    ** Important ** : FSEv08_decompress() does not decompress non-compressible nor RLE data !!!
    Why ? : making this distinction requires a header.
    Header management is intentionally delegated to the user layer, which can better manage special cases.
*/
size_t FSEv08_decompress(void* dst,  size_t dstCapacity,
                const void* cSrc, size_t cSrcSize);


/*-*****************************************
*  Tool functions
******************************************/
/* Error Management */
unsigned    FSEv08_isError(size_t code);        /* tells if a return value is an error code */
const char* FSEv08_getErrorName(size_t code);   /* provides error code string (useful for debugging) */


/*-*****************************************
*  FSE detailed API
******************************************/
/*!
FSEv08_decompress() does the following:
1. read normalized counters with readNCount()
2. build decoding table 'DTable' from normalized counters
3. decode the data stream using decoding table 'DTable'

The following API allows targeting specific sub-functions for advanced tasks.
For example, it's possible to compress several blocks using the same 'CTable',
or to save and provide normalized distribution using external method.
*/


/* *** DECOMPRESSION *** */

/*! FSEv08_readNCount():
    Read compactly saved 'normalizedCounter' from 'rBuffer'.
    @return : size read from 'rBuffer',
              or an errorCode, which can be tested using FSEv08_isError().
              maxSymbolValuePtr[0] and tableLogPtr[0] will also be updated with their respective values */
size_t FSEv08_readNCount (short* normalizedCounter, unsigned* maxSymbolValuePtr, unsigned* tableLogPtr, const void* rBuffer, size_t rBuffSize);

/*! Constructor and Destructor of FSEv08_DTable.
    Note that its size depends on 'tableLog' */
typedef unsigned FSEv08_DTable;   /* don't allocate that. It's just a way to be more restrictive than void* */
FSEv08_DTable* FSEv08_createDTable(unsigned tableLog);
void        FSEv08_freeDTable(FSEv08_DTable* dt);

/*! FSEv08_buildDTable():
    Builds 'dt', which must be already allocated, using FSEv08_createDTable().
    return : 0, or an errorCode, which can be tested using FSEv08_isError() */
size_t FSEv08_buildDTable (FSEv08_DTable* dt, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog);

/*! FSEv08_decompress_usingDTable():
    Decompress compressed source `cSrc` of size `cSrcSize` using `dt`
    into `dst` which must be already allocated.
    @return : size of regenerated data (necessarily <= `dstCapacity`),
              or an errorCode, which can be tested using FSEv08_isError() */
size_t FSEv08_decompress_usingDTable(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, const FSEv08_DTable* dt);

/*!
Tutorial :
----------
(Note : these functions only decompress FSE-compressed blocks.
 If block is uncompressed, use memcpy() instead
 If block is a single repeated byte, use memset() instead )

The first step is to obtain the normalized frequencies of symbols.
This can be performed by FSEv08_readNCount() if it was saved using FSEv08_writeNCount().
'normalizedCounter' must be already allocated, and have at least 'maxSymbolValuePtr[0]+1' cells of signed short.
In practice, that means it's necessary to know 'maxSymbolValue' beforehand,
or size the table to handle worst case situations (typically 256).
FSEv08_readNCount() will provide 'tableLog' and 'maxSymbolValue'.
The result of FSEv08_readNCount() is the number of bytes read from 'rBuffer'.
Note that 'rBufferSize' must be at least 4 bytes, even if useful information is less than that.
If there is an error, the function will return an error code, which can be tested using FSEv08_isError().

The next step is to build the decompression tables 'FSEv08_DTable' from 'normalizedCounter'.
This is performed by the function FSEv08_buildDTable().
The space required by 'FSEv08_DTable' must be already allocated using FSEv08_createDTable().
If there is an error, the function will return an error code, which can be tested using FSEv08_isError().

`FSEv08_DTable` can then be used to decompress `cSrc`, with FSEv08_decompress_usingDTable().
`cSrcSize` must be strictly correct, otherwise decompression will fail.
FSEv08_decompress_usingDTable() result will tell how many bytes were regenerated (<=`dstCapacity`).
If there is an error, the function will return an error code, which can be tested using FSEv08_isError(). (ex: dst buffer too small)
*/


#ifdef FSEv08_STATIC_LINKING_ONLY


/* *****************************************
*  Static allocation
*******************************************/
/* FSE buffer bounds */
#define FSEv08_NCOUNTBOUND 512
#define FSEv08_BLOCKBOUND(size) (size + (size>>7))

/* It is possible to statically allocate FSE CTable/DTable as a table of unsigned using below macros */
#define FSEv08_DTABLE_SIZE_U32(maxTableLog)                   (1 + (1<<maxTableLog))


/* *****************************************
*  FSE advanced API
*******************************************/
size_t FSEv08_countFast(unsigned* count, unsigned* maxSymbolValuePtr, const void* src, size_t srcSize);
/**< same as FSEv08_count(), but blindly trusts that all byte values within src are <= *maxSymbolValuePtr  */

unsigned FSEv08_optimalTableLog_internal(unsigned maxTableLog, size_t srcSize, unsigned maxSymbolValue, unsigned minus);
/**< same as FSEv08_optimalTableLog(), which used `minus==2` */

size_t FSEv08_buildDTable_raw (FSEv08_DTable* dt, unsigned nbBits);
/**< build a fake FSEv08_DTable, designed to read an uncompressed bitstream where each symbol uses nbBits */

size_t FSEv08_buildDTable_rle (FSEv08_DTable* dt, unsigned char symbolValue);
/**< build a fake FSEv08_DTable, designed to always generate the same symbolValue */



/* *****************************************
*  FSE symbol decompression API
*******************************************/
typedef struct
{
    size_t      state;
    const void* table;   /* precise table may vary, depending on U16 */
} FSEv08_DState_t;


static void     FSEv08_initDState(FSEv08_DState_t* DStatePtr, BIT_DStream_t* bitD, const FSEv08_DTable* dt);

static unsigned char FSEv08_decodeSymbol(FSEv08_DState_t* DStatePtr, BIT_DStream_t* bitD);

static unsigned FSEv08_endOfDState(const FSEv08_DState_t* DStatePtr);

/**<
Let's now decompose FSEv08_decompress_usingDTable() into its unitary components.
You will decode FSE-encoded symbols from the bitStream,
and also any other bitFields you put in, **in reverse order**.

You will need a few variables to track your bitStream. They are :

BIT_DStream_t DStream;    // Stream context
FSEv08_DState_t  DState;     // State context. Multiple ones are possible
FSEv08_DTable*   DTablePtr;  // Decoding table, provided by FSEv08_buildDTable()

The first thing to do is to init the bitStream.
    errorCode = BIT_initDStream(&DStream, srcBuffer, srcSize);

You should then retrieve your initial state(s)
(in reverse flushing order if you have several ones) :
    errorCode = FSEv08_initDState(&DState, &DStream, DTablePtr);

You can then decode your data, symbol after symbol.
For information the maximum number of bits read by FSEv08_decodeSymbol() is 'tableLog'.
Keep in mind that symbols are decoded in reverse order, like a LIFO stack (last in, first out).
    unsigned char symbol = FSEv08_decodeSymbol(&DState, &DStream);

You can retrieve any bitfield you eventually stored into the bitStream (in reverse order)
Note : maximum allowed nbBits is 25, for 32-bits compatibility
    size_t bitField = BIT_readBits(&DStream, nbBits);

All above operations only read from local register (which size depends on size_t).
Refueling the register from memory is manually performed by the reload method.
    endSignal = FSEv08_reloadDStream(&DStream);

BIT_reloadDStream() result tells if there is still some more data to read from DStream.
BIT_DStream_unfinished : there is still some data left into the DStream.
BIT_DStream_endOfBuffer : Dstream reached end of buffer. Its container may no longer be completely filled.
BIT_DStream_completed : Dstream reached its exact end, corresponding in general to decompression completed.
BIT_DStream_tooFar : Dstream went too far. Decompression result is corrupted.

When reaching end of buffer (BIT_DStream_endOfBuffer), progress slowly, notably if you decode multiple symbols per loop,
to properly detect the exact end of stream.
After each decoded symbol, check if DStream is fully consumed using this simple test :
    BIT_reloadDStream(&DStream) >= BIT_DStream_completed

When it's done, verify decompression is fully completed, by checking both DStream and the relevant states.
Checking if DStream has reached its end is performed by :
    BIT_endOfDStream(&DStream);
Check also the states. There might be some symbols left there, if some high probability ones (>50%) are possible.
    FSEv08_endOfDState(&DState);
*/


/* *****************************************
*  FSE unsafe API
*******************************************/
static unsigned char FSEv08_decodeSymbolFast(FSEv08_DState_t* DStatePtr, BIT_DStream_t* bitD);
/* faster, but works only if nbBits is always >= 1 (otherwise, result will be corrupted) */


/* *****************************************
*  Implementation of inlined functions
*******************************************/
/*<=====    Decompression    =====>*/

typedef struct {
    U16 tableLog;
    U16 fastMode;
} FSEv08_DTableHeader;   /* sizeof U32 */

typedef struct
{
    unsigned short newState;
    unsigned char  symbol;
    unsigned char  nbBits;
} FSEv08_decode_t;   /* size == U32 */

MEM_STATIC void FSEv08_initDState(FSEv08_DState_t* DStatePtr, BIT_DStream_t* bitD, const FSEv08_DTable* dt)
{
    const void* ptr = dt;
    const FSEv08_DTableHeader* const DTableH = (const FSEv08_DTableHeader*)ptr;
    DStatePtr->state = BIT_readBits(bitD, DTableH->tableLog);
    BIT_reloadDStream(bitD);
    DStatePtr->table = dt + 1;
}

MEM_STATIC BYTE FSEv08_peekSymbol(const FSEv08_DState_t* DStatePtr)
{
    FSEv08_decode_t const DInfo = ((const FSEv08_decode_t*)(DStatePtr->table))[DStatePtr->state];
    return DInfo.symbol;
}

MEM_STATIC void FSEv08_updateState(FSEv08_DState_t* DStatePtr, BIT_DStream_t* bitD)
{
    FSEv08_decode_t const DInfo = ((const FSEv08_decode_t*)(DStatePtr->table))[DStatePtr->state];
    U32 const nbBits = DInfo.nbBits;
    size_t const lowBits = BIT_readBits(bitD, nbBits);
    DStatePtr->state = DInfo.newState + lowBits;
}

MEM_STATIC BYTE FSEv08_decodeSymbol(FSEv08_DState_t* DStatePtr, BIT_DStream_t* bitD)
{
    FSEv08_decode_t const DInfo = ((const FSEv08_decode_t*)(DStatePtr->table))[DStatePtr->state];
    U32 const nbBits = DInfo.nbBits;
    BYTE const symbol = DInfo.symbol;
    size_t const lowBits = BIT_readBits(bitD, nbBits);

    DStatePtr->state = DInfo.newState + lowBits;
    return symbol;
}

/*! FSEv08_decodeSymbolFast() :
    unsafe, only works if no symbol has a probability > 50% */
MEM_STATIC BYTE FSEv08_decodeSymbolFast(FSEv08_DState_t* DStatePtr, BIT_DStream_t* bitD)
{
    FSEv08_decode_t const DInfo = ((const FSEv08_decode_t*)(DStatePtr->table))[DStatePtr->state];
    U32 const nbBits = DInfo.nbBits;
    BYTE const symbol = DInfo.symbol;
    size_t const lowBits = BIT_readBitsFast(bitD, nbBits);

    DStatePtr->state = DInfo.newState + lowBits;
    return symbol;
}

MEM_STATIC unsigned FSEv08_endOfDState(const FSEv08_DState_t* DStatePtr)
{
    return DStatePtr->state == 0;
}



#ifndef FSEv08_COMMONDEFS_ONLY

/* **************************************************************
*  Tuning parameters
****************************************************************/
/*!MEMORY_USAGE :
*  Memory usage formula : N->2^N Bytes (examples : 10 -> 1KB; 12 -> 4KB ; 16 -> 64KB; 20 -> 1MB; etc.)
*  Increasing memory usage improves compression ratio
*  Reduced memory usage can improve speed, due to cache effect
*  Recommended max value is 14, for 16KB, which nicely fits into Intel x86 L1 cache */
#define FSEv08_MAX_MEMORY_USAGE 14
#define FSEv08_DEFAULT_MEMORY_USAGE 13

/*!FSEv08_MAX_SYMBOL_VALUE :
*  Maximum symbol value authorized.
*  Required for proper stack allocation */
#define FSEv08_MAX_SYMBOL_VALUE 255


/* **************************************************************
*  template functions type & suffix
****************************************************************/
#define FSEv08_FUNCTION_TYPE BYTE
#define FSEv08_FUNCTION_EXTENSION
#define FSEv08_DECODE_TYPE FSEv08_decode_t


#endif   /* !FSEv08_COMMONDEFS_ONLY */


/* ***************************************************************
*  Constants
*****************************************************************/
#define FSEv08_MAX_TABLELOG  (FSEv08_MAX_MEMORY_USAGE-2)
#define FSEv08_MAX_TABLESIZE (1U<<FSEv08_MAX_TABLELOG)
#define FSEv08_MAXTABLESIZE_MASK (FSEv08_MAX_TABLESIZE-1)
#define FSEv08_DEFAULT_TABLELOG (FSEv08_DEFAULT_MEMORY_USAGE-2)
#define FSEv08_MIN_TABLELOG 5

#define FSEv08_TABLELOG_ABSOLUTE_MAX 15
#if FSEv08_MAX_TABLELOG > FSEv08_TABLELOG_ABSOLUTE_MAX
#  error "FSEv08_MAX_TABLELOG > FSEv08_TABLELOG_ABSOLUTE_MAX is not supported"
#endif

#define FSEv08_TABLESTEP(tableSize) ((tableSize>>1) + (tableSize>>3) + 3)


#endif /* FSEv08_STATIC_LINKING_ONLY */


#if defined (__cplusplus)
}
#endif

#endif  /* FSEv08_H */
/* ******************************************************************
   Huffman coder, part of New Generation Entropy library
   header file
   Copyright (C) 2013-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
****************************************************************** */
#ifndef HUFv08_H_298734234
#define HUFv08_H_298734234

#if defined (__cplusplus)
extern "C" {
#endif


/* *** simple functions *** */
/**
HUFv08_decompress() :
    Decompress HUF data from buffer 'cSrc', of size 'cSrcSize',
    into already allocated buffer 'dst', of minimum size 'dstSize'.
    `dstSize` : **must** be the ***exact*** size of original (uncompressed) data.
    Note : in contrast with FSE, HUFv08_decompress can regenerate
           RLE (cSrcSize==1) and uncompressed (cSrcSize==dstSize) data,
           because it knows size to regenerate.
    @return : size of regenerated data (== dstSize),
              or an error code, which can be tested using HUFv08_isError()
*/
size_t HUFv08_decompress(void* dst,  size_t dstSize,
                const void* cSrc, size_t cSrcSize);


/* ****************************************
*  Tool functions
******************************************/
#define HUFv08_BLOCKSIZE_MAX (128 * 1024)

/* Error Management */
unsigned    HUFv08_isError(size_t code);        /**< tells if a return value is an error code */
const char* HUFv08_getErrorName(size_t code);   /**< provides error code string (useful for debugging) */


/* *** Advanced function *** */


#ifdef HUFv08_STATIC_LINKING_ONLY


/* *** Constants *** */
#define HUFv08_TABLELOG_ABSOLUTEMAX  16   /* absolute limit of HUFv08_MAX_TABLELOG. Beyond that value, code does not work */
#define HUFv08_TABLELOG_MAX  12           /* max configured tableLog (for static allocation); can be modified up to HUFv08_ABSOLUTEMAX_TABLELOG */
#define HUFv08_TABLELOG_DEFAULT  11       /* tableLog by default, when not specified */
#define HUFv08_SYMBOLVALUE_MAX 255
#if (HUFv08_TABLELOG_MAX > HUFv08_TABLELOG_ABSOLUTEMAX)
#  error "HUFv08_TABLELOG_MAX is too large !"
#endif


/* ****************************************
*  Static allocation
******************************************/
/* HUF buffer bounds */
#define HUFv08_BLOCKBOUND(size) (size + (size>>8) + 8)   /* only true if incompressible pre-filtered with fast heuristic */

/* static allocation of HUF's DTable */
typedef U32 HUFv08_DTable;
#define HUFv08_DTABLE_SIZE(maxTableLog)   (1 + (1<<(maxTableLog)))
#define HUFv08_CREATE_STATIC_DTABLEX2(DTable, maxTableLog) \
        HUFv08_DTable DTable[HUFv08_DTABLE_SIZE((maxTableLog)-1)] = { ((U32)((maxTableLog)-1)*0x1000001) }
#define HUFv08_CREATE_STATIC_DTABLEX4(DTable, maxTableLog) \
        HUFv08_DTable DTable[HUFv08_DTABLE_SIZE(maxTableLog)] = { ((U32)(maxTableLog)*0x1000001) }


/* ****************************************
*  Advanced decompression functions
******************************************/
size_t HUFv08_decompress4X2 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /**< single-symbol decoder */
size_t HUFv08_decompress4X4 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /**< double-symbols decoder */

size_t HUFv08_decompress4X_DCtx (HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /**< decodes RLE and uncompressed */
size_t HUFv08_decompress4X_hufOnly(HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize); /**< considers RLE and uncompressed as errors */
size_t HUFv08_decompress4X2_DCtx(HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /**< single-symbol decoder */
size_t HUFv08_decompress4X4_DCtx(HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /**< double-symbols decoder */

size_t HUFv08_decompress1X_DCtx (HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);
size_t HUFv08_decompress1X2_DCtx(HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /**< single-symbol decoder */
size_t HUFv08_decompress1X4_DCtx(HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /**< double-symbols decoder */


/* ****************************************
*  HUF detailed API
******************************************/
/*
HUFv08_decompress() does the following:
1. select the decompression algorithm (X2, X4) based on pre-computed heuristics
2. build Huffman table from save, using HUFv08_readDTableXn()
3. decode 1 or 4 segments in parallel using HUFv08_decompressSXn_usingDTable
*/

/** HUFv08_selectDecoder() :
*   Tells which decoder is likely to decode faster,
*   based on a set of pre-determined metrics.
*   @return : 0==HUFv08_decompress4X2, 1==HUFv08_decompress4X4 .
*   Assumption : 0 < cSrcSize < dstSize <= 128 KB */
U32 HUFv08_selectDecoder (size_t dstSize, size_t cSrcSize);

size_t HUFv08_readDTableX2 (HUFv08_DTable* DTable, const void* src, size_t srcSize);
size_t HUFv08_readDTableX4 (HUFv08_DTable* DTable, const void* src, size_t srcSize);

size_t HUFv08_decompress4X_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFv08_DTable* DTable);
size_t HUFv08_decompress4X2_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFv08_DTable* DTable);
size_t HUFv08_decompress4X4_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFv08_DTable* DTable);


/* single stream variants */

size_t HUFv08_decompress1X2 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /* single-symbol decoder */
size_t HUFv08_decompress1X4 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /* double-symbol decoder */

size_t HUFv08_decompress1X_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFv08_DTable* DTable);
size_t HUFv08_decompress1X2_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFv08_DTable* DTable);
size_t HUFv08_decompress1X4_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFv08_DTable* DTable);


#endif /* HUFv08_STATIC_LINKING_ONLY */


#if defined (__cplusplus)
}
#endif

#endif   /* HUFv08_H_298734234 */
/*
   Common functions of New Generation Entropy library
   Copyright (C) 2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - FSE+HUF source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
*************************************************************************** */


/*-****************************************
*  FSE Error Management
******************************************/
unsigned FSEv08_isError(size_t code) { return ERR_isError(code); }

const char* FSEv08_getErrorName(size_t code) { return ERR_getErrorName(code); }


/* **************************************************************
*  HUF Error Management
****************************************************************/
unsigned HUFv08_isError(size_t code) { return ERR_isError(code); }

const char* HUFv08_getErrorName(size_t code) { return ERR_getErrorName(code); }


/*-**************************************************************
*  FSE NCount encoding-decoding
****************************************************************/
static short FSEv08_abs(short a) { return (short)(a<0 ? -a : a); }

size_t FSEv08_readNCount (short* normalizedCounter, unsigned* maxSVPtr, unsigned* tableLogPtr,
                 const void* headerBuffer, size_t hbSize)
{
    const BYTE* const istart = (const BYTE*) headerBuffer;
    const BYTE* const iend = istart + hbSize;
    const BYTE* ip = istart;
    int nbBits;
    int remaining;
    int threshold;
    U32 bitStream;
    int bitCount;
    unsigned charnum = 0;
    int previous0 = 0;

    if (hbSize < 4) return ERROR(srcSize_wrong);
    bitStream = MEM_readLE32(ip);
    nbBits = (bitStream & 0xF) + FSEv08_MIN_TABLELOG;   /* extract tableLog */
    if (nbBits > FSEv08_TABLELOG_ABSOLUTE_MAX) return ERROR(tableLog_tooLarge);
    bitStream >>= 4;
    bitCount = 4;
    *tableLogPtr = nbBits;
    remaining = (1<<nbBits)+1;
    threshold = 1<<nbBits;
    nbBits++;

    while ((remaining>1) & (charnum<=*maxSVPtr)) {
        if (previous0) {
            unsigned n0 = charnum;
            while ((bitStream & 0xFFFF) == 0xFFFF) {
                n0 += 24;
                if (ip < iend-5) {
                    ip += 2;
                    bitStream = MEM_readLE32(ip) >> bitCount;
                } else {
                    bitStream >>= 16;
                    bitCount   += 16;
            }   }
            while ((bitStream & 3) == 3) {
                n0 += 3;
                bitStream >>= 2;
                bitCount += 2;
            }
            n0 += bitStream & 3;
            bitCount += 2;
            if (n0 > *maxSVPtr) return ERROR(maxSymbolValue_tooSmall);
            while (charnum < n0) normalizedCounter[charnum++] = 0;
            if ((ip <= iend-7) || (ip + (bitCount>>3) <= iend-4)) {
                ip += bitCount>>3;
                bitCount &= 7;
                bitStream = MEM_readLE32(ip) >> bitCount;
            } else {
                bitStream >>= 2;
        }   }
        {   short const max = (short)((2*threshold-1)-remaining);
            short count;

            if ((bitStream & (threshold-1)) < (U32)max) {
                count = (short)(bitStream & (threshold-1));
                bitCount   += nbBits-1;
            } else {
                count = (short)(bitStream & (2*threshold-1));
                if (count >= threshold) count -= max;
                bitCount   += nbBits;
            }

            count--;   /* extra accuracy */
            remaining -= FSEv08_abs(count);
            normalizedCounter[charnum++] = count;
            previous0 = !count;
            while (remaining < threshold) {
                nbBits--;
                threshold >>= 1;
            }

            if ((ip <= iend-7) || (ip + (bitCount>>3) <= iend-4)) {
                ip += bitCount>>3;
                bitCount &= 7;
            } else {
                bitCount -= (int)(8 * (iend - 4 - ip));
                ip = iend - 4;
            }
            bitStream = MEM_readLE32(ip) >> (bitCount & 31);
    }   }   /* while ((remaining>1) & (charnum<=*maxSVPtr)) */
    if (remaining != 1) return ERROR(corruption_detected);
    if (bitCount > 32) return ERROR(corruption_detected);
    *maxSVPtr = charnum-1;

    ip += (bitCount+7)>>3;
    return ip-istart;
}


/*! HUFv08_readStats() :
    Read compact Huffman tree, saved by HUFv08_writeCTable().
    `huffWeight` is destination buffer.
    @return : size read from `src` , or an error Code .
    Note : Needed by HUFv08_readCTable() and HUFv08_readDTableX?() .
*/
size_t HUFv08_readStats(BYTE* huffWeight, size_t hwSize, U32* rankStats,
                     U32* nbSymbolsPtr, U32* tableLogPtr,
                     const void* src, size_t srcSize)
{
    U32 weightTotal;
    const BYTE* ip = (const BYTE*) src;
    size_t iSize = ip[0];
    size_t oSize;

    /* memset(huffWeight, 0, hwSize);   *//* is not necessary, even though some analyzer complain ... */

    if (iSize >= 128) {  /* special header */
        oSize = iSize - 127;
        iSize = ((oSize+1)/2);
        if (iSize+1 > srcSize) return ERROR(srcSize_wrong);
        if (oSize >= hwSize) return ERROR(corruption_detected);
        ip += 1;
        {   U32 n;
            for (n=0; n<oSize; n+=2) {
                huffWeight[n]   = ip[n/2] >> 4;
                huffWeight[n+1] = ip[n/2] & 15;
    }   }   }
    else  {   /* header compressed with FSE (normal case) */
        if (iSize+1 > srcSize) return ERROR(srcSize_wrong);
        oSize = FSEv08_decompress(huffWeight, hwSize-1, ip+1, iSize);   /* max (hwSize-1) values decoded, as last one is implied */
        if (FSEv08_isError(oSize)) return oSize;
    }

    /* collect weight stats */
    memset(rankStats, 0, (HUFv08_TABLELOG_ABSOLUTEMAX + 1) * sizeof(U32));
    weightTotal = 0;
    {   U32 n; for (n=0; n<oSize; n++) {
            if (huffWeight[n] >= HUFv08_TABLELOG_ABSOLUTEMAX) return ERROR(corruption_detected);
            rankStats[huffWeight[n]]++;
            weightTotal += (1 << huffWeight[n]) >> 1;
    }   }

    /* get last non-null symbol weight (implied, total must be 2^n) */
    {   U32 const tableLog = BIT_highbit32(weightTotal) + 1;
        if (tableLog > HUFv08_TABLELOG_ABSOLUTEMAX) return ERROR(corruption_detected);
        *tableLogPtr = tableLog;
        /* determine last weight */
        {   U32 const total = 1 << tableLog;
            U32 const rest = total - weightTotal;
            U32 const verif = 1 << BIT_highbit32(rest);
            U32 const lastWeight = BIT_highbit32(rest) + 1;
            if (verif != rest) return ERROR(corruption_detected);    /* last value must be a clean power of 2 */
            huffWeight[oSize] = (BYTE)lastWeight;
            rankStats[lastWeight]++;
    }   }

    /* check tree construction validity */
    if ((rankStats[1] < 2) || (rankStats[1] & 1)) return ERROR(corruption_detected);   /* by construction : at least 2 elts of rank 1, must be even */

    /* results */
    *nbSymbolsPtr = (U32)(oSize+1);
    return iSize+1;
}
/* ******************************************************************
   FSE : Finite State Entropy decoder
   Copyright (C) 2013-2015, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */


/* **************************************************************
*  Compiler specifics
****************************************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define FORCE_INLINE static __forceinline
#  include <intrin.h>                    /* For Visual 2005 */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4214)        /* disable: C4214: non-int bitfields */
#else
#  ifdef __GNUC__
#    define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#    define FORCE_INLINE static inline __attribute__((always_inline))
#  else
#    define FORCE_INLINE static inline
#  endif
#endif


/* **************************************************************
*  Error Management
****************************************************************/
#define FSEv08_isError ERR_isError
#define FSEv08_STATIC_ASSERT(c) { enum { FSEv08_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable declarations */


/* **************************************************************
*  Complex types
****************************************************************/
typedef U32 DTable_max_t[FSEv08_DTABLE_SIZE_U32(FSEv08_MAX_TABLELOG)];


/* **************************************************************
*  Templates
****************************************************************/
/*
  designed to be included
  for type-specific functions (template emulation in C)
  Objective is to write these functions only once, for improved maintenance
*/

/* safety checks */
#ifndef FSEv08_FUNCTION_EXTENSION
#  error "FSEv08_FUNCTION_EXTENSION must be defined"
#endif
#ifndef FSEv08_FUNCTION_TYPE
#  error "FSEv08_FUNCTION_TYPE must be defined"
#endif

/* Function names */
#define FSEv08_CAT(X,Y) X##Y
#define FSEv08_FUNCTION_NAME(X,Y) FSEv08_CAT(X,Y)
#define FSEv08_TYPE_NAME(X,Y) FSEv08_CAT(X,Y)


/* Function templates */
FSEv08_DTable* FSEv08_createDTable (unsigned tableLog)
{
    if (tableLog > FSEv08_TABLELOG_ABSOLUTE_MAX) tableLog = FSEv08_TABLELOG_ABSOLUTE_MAX;
    return (FSEv08_DTable*)malloc( FSEv08_DTABLE_SIZE_U32(tableLog) * sizeof (U32) );
}

void FSEv08_freeDTable (FSEv08_DTable* dt)
{
    free(dt);
}

size_t FSEv08_buildDTable(FSEv08_DTable* dt, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog)
{
    void* const tdPtr = dt+1;   /* because *dt is unsigned, 32-bits aligned on 32-bits */
    FSEv08_DECODE_TYPE* const tableDecode = (FSEv08_DECODE_TYPE*) (tdPtr);
    U16 symbolNext[FSEv08_MAX_SYMBOL_VALUE+1];

    U32 const maxSV1 = maxSymbolValue + 1;
    U32 const tableSize = 1 << tableLog;
    U32 highThreshold = tableSize-1;

    /* Sanity Checks */
    if (maxSymbolValue > FSEv08_MAX_SYMBOL_VALUE) return ERROR(maxSymbolValue_tooLarge);
    if (tableLog > FSEv08_MAX_TABLELOG) return ERROR(tableLog_tooLarge);

    /* Init, lay down lowprob symbols */
    {   FSEv08_DTableHeader DTableH;
        DTableH.tableLog = (U16)tableLog;
        DTableH.fastMode = 1;
        {   S16 const largeLimit= (S16)(1 << (tableLog-1));
            U32 s;
            for (s=0; s<maxSV1; s++) {
                if (normalizedCounter[s]==-1) {
                    tableDecode[highThreshold--].symbol = (FSEv08_FUNCTION_TYPE)s;
                    symbolNext[s] = 1;
                } else {
                    if (normalizedCounter[s] >= largeLimit) DTableH.fastMode=0;
                    symbolNext[s] = normalizedCounter[s];
        }   }   }
        memcpy(dt, &DTableH, sizeof(DTableH));
    }

    /* Spread symbols */
    {   U32 const tableMask = tableSize-1;
        U32 const step = FSEv08_TABLESTEP(tableSize);
        U32 s, position = 0;
        for (s=0; s<maxSV1; s++) {
            int i;
            for (i=0; i<normalizedCounter[s]; i++) {
                tableDecode[position].symbol = (FSEv08_FUNCTION_TYPE)s;
                position = (position + step) & tableMask;
                while (position > highThreshold) position = (position + step) & tableMask;   /* lowprob area */
        }   }

        if (position!=0) return ERROR(GENERIC);   /* position must reach all cells once, otherwise normalizedCounter is incorrect */
    }

    /* Build Decoding table */
    {   U32 u;
        for (u=0; u<tableSize; u++) {
            FSEv08_FUNCTION_TYPE const symbol = (FSEv08_FUNCTION_TYPE)(tableDecode[u].symbol);
            U16 nextState = symbolNext[symbol]++;
            tableDecode[u].nbBits = (BYTE) (tableLog - BIT_highbit32 ((U32)nextState) );
            tableDecode[u].newState = (U16) ( (nextState << tableDecode[u].nbBits) - tableSize);
    }   }

    return 0;
}



#ifndef FSEv08_COMMONDEFS_ONLY

/*-*******************************************************
*  Decompression (Byte symbols)
*********************************************************/
size_t FSEv08_buildDTable_rle (FSEv08_DTable* dt, BYTE symbolValue)
{
    void* ptr = dt;
    FSEv08_DTableHeader* const DTableH = (FSEv08_DTableHeader*)ptr;
    void* dPtr = dt + 1;
    FSEv08_decode_t* const cell = (FSEv08_decode_t*)dPtr;

    DTableH->tableLog = 0;
    DTableH->fastMode = 0;

    cell->newState = 0;
    cell->symbol = symbolValue;
    cell->nbBits = 0;

    return 0;
}


size_t FSEv08_buildDTable_raw (FSEv08_DTable* dt, unsigned nbBits)
{
    void* ptr = dt;
    FSEv08_DTableHeader* const DTableH = (FSEv08_DTableHeader*)ptr;
    void* dPtr = dt + 1;
    FSEv08_decode_t* const dinfo = (FSEv08_decode_t*)dPtr;
    const unsigned tableSize = 1 << nbBits;
    const unsigned tableMask = tableSize - 1;
    const unsigned maxSV1 = tableMask+1;
    unsigned s;

    /* Sanity checks */
    if (nbBits < 1) return ERROR(GENERIC);         /* min size */

    /* Build Decoding Table */
    DTableH->tableLog = (U16)nbBits;
    DTableH->fastMode = 1;
    for (s=0; s<maxSV1; s++) {
        dinfo[s].newState = 0;
        dinfo[s].symbol = (BYTE)s;
        dinfo[s].nbBits = (BYTE)nbBits;
    }

    return 0;
}

FORCE_INLINE size_t FSEv08_decompress_usingDTable_generic(
          void* dst, size_t maxDstSize,
    const void* cSrc, size_t cSrcSize,
    const FSEv08_DTable* dt, const unsigned fast)
{
    BYTE* const ostart = (BYTE*) dst;
    BYTE* op = ostart;
    BYTE* const omax = op + maxDstSize;
    BYTE* const olimit = omax-3;

    BIT_DStream_t bitD;
    FSEv08_DState_t state1;
    FSEv08_DState_t state2;

    /* Init */
    { size_t const errorCode = BIT_initDStream(&bitD, cSrc, cSrcSize);   /* replaced last arg by maxCompressed Size */
      if (FSEv08_isError(errorCode)) return errorCode; }

    FSEv08_initDState(&state1, &bitD, dt);
    FSEv08_initDState(&state2, &bitD, dt);

#define FSEv08_GETSYMBOL(statePtr) fast ? FSEv08_decodeSymbolFast(statePtr, &bitD) : FSEv08_decodeSymbol(statePtr, &bitD)

    /* 4 symbols per loop */
    for ( ; (BIT_reloadDStream(&bitD)==BIT_DStream_unfinished) && (op<olimit) ; op+=4) {
        op[0] = FSEv08_GETSYMBOL(&state1);

        if (FSEv08_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            BIT_reloadDStream(&bitD);

        op[1] = FSEv08_GETSYMBOL(&state2);

        if (FSEv08_MAX_TABLELOG*4+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            { if (BIT_reloadDStream(&bitD) > BIT_DStream_unfinished) { op+=2; break; } }

        op[2] = FSEv08_GETSYMBOL(&state1);

        if (FSEv08_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            BIT_reloadDStream(&bitD);

        op[3] = FSEv08_GETSYMBOL(&state2);
    }

    /* tail */
    /* note : BIT_reloadDStream(&bitD) >= FSEv08_DStream_partiallyFilled; Ends at exactly BIT_DStream_completed */
    while (1) {
        if (op>(omax-2)) return ERROR(dstSize_tooSmall);

        *op++ = FSEv08_GETSYMBOL(&state1);

        if (BIT_reloadDStream(&bitD)==BIT_DStream_overflow) {
            *op++ = FSEv08_GETSYMBOL(&state2);
            break;
        }

        if (op>(omax-2)) return ERROR(dstSize_tooSmall);

        *op++ = FSEv08_GETSYMBOL(&state2);

        if (BIT_reloadDStream(&bitD)==BIT_DStream_overflow) {
            *op++ = FSEv08_GETSYMBOL(&state1);
            break;
    }   }

    return op-ostart;
}


size_t FSEv08_decompress_usingDTable(void* dst, size_t originalSize,
                            const void* cSrc, size_t cSrcSize,
                            const FSEv08_DTable* dt)
{
    const void* ptr = dt;
    const FSEv08_DTableHeader* DTableH = (const FSEv08_DTableHeader*)ptr;
    const U32 fastMode = DTableH->fastMode;

    /* select fast mode (static) */
    if (fastMode) return FSEv08_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 1);
    return FSEv08_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 0);
}


size_t FSEv08_decompress(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize)
{
    const BYTE* const istart = (const BYTE*)cSrc;
    const BYTE* ip = istart;
    short counting[FSEv08_MAX_SYMBOL_VALUE+1];
    DTable_max_t dt;   /* Static analyzer seems unable to understand this table will be properly initialized later */
    unsigned tableLog;
    unsigned maxSymbolValue = FSEv08_MAX_SYMBOL_VALUE;

    if (cSrcSize<2) return ERROR(srcSize_wrong);   /* too small input size */

    /* normal FSE decoding mode */
    {   size_t const NCountLength = FSEv08_readNCount (counting, &maxSymbolValue, &tableLog, istart, cSrcSize);
        if (FSEv08_isError(NCountLength)) return NCountLength;
        if (NCountLength >= cSrcSize) return ERROR(srcSize_wrong);   /* too small input size */
        ip += NCountLength;
        cSrcSize -= NCountLength;
    }

    { size_t const errorCode = FSEv08_buildDTable (dt, counting, maxSymbolValue, tableLog);
      if (FSEv08_isError(errorCode)) return errorCode; }

    return FSEv08_decompress_usingDTable (dst, maxDstSize, ip, cSrcSize, dt);   /* always return, even if it is an error code */
}



#endif   /* FSEv08_COMMONDEFS_ONLY */
/*
    Common functions of Zstd compression library
    Copyright (C) 2015-2016, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - zstd homepage : http://www.zstd.net/
*/



/*-****************************************
*  ZSTD Error Management
******************************************/
/*! ZSTDv08_isError() :
*   tells if a return value is an error code */
unsigned ZSTDv08_isError(size_t code) { return ERR_isError(code); }

/*! ZSTDv08_getErrorName() :
*   provides error code string from function result (useful for debugging) */
const char* ZSTDv08_getErrorName(size_t code) { return ERR_getErrorName(code); }

/*! ZSTDv08_getError() :
*   convert a `size_t` function result into a proper ZSTDv08_errorCode enum */
ZSTDv08_ErrorCode ZSTDv08_getErrorCode(size_t code) { return ERR_getErrorCode(code); }

/*! ZSTDv08_getErrorString() :
*   provides error code string from enum */
const char* ZSTDv08_getErrorString(ZSTDv08_ErrorCode code) { return ERR_getErrorName(code); }


/* **************************************************************
*  ZBUFF Error Management
****************************************************************/
unsigned ZBUFFv08_isError(size_t errorCode) { return ERR_isError(errorCode); }

const char* ZBUFFv08_getErrorName(size_t errorCode) { return ERR_getErrorName(errorCode); }



void* ZSTDv08_defaultAllocFunction(void* opaque, size_t size)
{
    void* address = malloc(size);
    (void)opaque;
    /* printf("alloc %p, %d opaque=%p \n", address, (int)size, opaque); */
    return address;
}

void ZSTDv08_defaultFreeFunction(void* opaque, void* address)
{
    (void)opaque;
    /* if (address) printf("free %p opaque=%p \n", address, opaque); */
    free(address);
}
/* ******************************************************************
   Huffman decoder, part of New Generation Entropy library
   Copyright (C) 2013-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - FSE+HUF source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */

/* **************************************************************
*  Compiler specifics
****************************************************************/
#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
/* inline is defined */
#elif defined(_MSC_VER)
#  define inline __inline
#else
#  define inline /* disable inline */
#endif


#ifdef _MSC_VER    /* Visual Studio */
#  define FORCE_INLINE static __forceinline
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#else
#  ifdef __GNUC__
#    define FORCE_INLINE static inline __attribute__((always_inline))
#  else
#    define FORCE_INLINE static inline
#  endif
#endif


/* **************************************************************
*  Error Management
****************************************************************/
#define HUFv08_STATIC_ASSERT(c) { enum { HUFv08_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable declarations */


/*-***************************/
/*  generic DTableDesc       */
/*-***************************/

typedef struct { BYTE maxTableLog; BYTE tableType; BYTE tableLog; BYTE reserved; } DTableDesc;

static DTableDesc HUFv08_getDTableDesc(const HUFv08_DTable* table)
{
    DTableDesc dtd;
    memcpy(&dtd, table, sizeof(dtd));
    return dtd;
}


/*-***************************/
/*  single-symbol decoding   */
/*-***************************/

typedef struct { BYTE byte; BYTE nbBits; } HUFv08_DEltX2;   /* single-symbol decoding */

size_t HUFv08_readDTableX2 (HUFv08_DTable* DTable, const void* src, size_t srcSize)
{
    BYTE huffWeight[HUFv08_SYMBOLVALUE_MAX + 1];
    U32 rankVal[HUFv08_TABLELOG_ABSOLUTEMAX + 1];   /* large enough for values from 0 to 16 */
    U32 tableLog = 0;
    U32 nbSymbols = 0;
    size_t iSize;
    void* const dtPtr = DTable + 1;
    HUFv08_DEltX2* const dt = (HUFv08_DEltX2*)dtPtr;

    HUFv08_STATIC_ASSERT(sizeof(DTableDesc) == sizeof(HUFv08_DTable));
    //memset(huffWeight, 0, sizeof(huffWeight));   /* is not necessary, even though some analyzer complain ... */

    iSize = HUFv08_readStats(huffWeight, HUFv08_SYMBOLVALUE_MAX + 1, rankVal, &nbSymbols, &tableLog, src, srcSize);
    if (HUFv08_isError(iSize)) return iSize;

    /* Table header */
    {   DTableDesc dtd = HUFv08_getDTableDesc(DTable);
        if (tableLog > (U32)(dtd.maxTableLog+1)) return ERROR(tableLog_tooLarge);   /* DTable too small, huffman tree cannot fit in */
        dtd.tableType = 0;
        dtd.tableLog = (BYTE)tableLog;
        memcpy(DTable, &dtd, sizeof(dtd));
    }

    /* Prepare ranks */
    {   U32 n, nextRankStart = 0;
        for (n=1; n<tableLog+1; n++) {
            U32 current = nextRankStart;
            nextRankStart += (rankVal[n] << (n-1));
            rankVal[n] = current;
    }   }

    /* fill DTable */
    {   U32 n;
        for (n=0; n<nbSymbols; n++) {
            U32 const w = huffWeight[n];
            U32 const length = (1 << w) >> 1;
            U32 i;
            HUFv08_DEltX2 D;
            D.byte = (BYTE)n; D.nbBits = (BYTE)(tableLog + 1 - w);
            for (i = rankVal[w]; i < rankVal[w] + length; i++)
                dt[i] = D;
            rankVal[w] += length;
    }   }

    return iSize;
}


static BYTE HUFv08_decodeSymbolX2(BIT_DStream_t* Dstream, const HUFv08_DEltX2* dt, const U32 dtLog)
{
    size_t const val = BIT_lookBitsFast(Dstream, dtLog); /* note : dtLog >= 1 */
    BYTE const c = dt[val].byte;
    BIT_skipBits(Dstream, dt[val].nbBits);
    return c;
}

#define HUFv08_DECODE_SYMBOLX2_0(ptr, DStreamPtr) \
    *ptr++ = HUFv08_decodeSymbolX2(DStreamPtr, dt, dtLog)

#define HUFv08_DECODE_SYMBOLX2_1(ptr, DStreamPtr) \
    if (MEM_64bits() || (HUFv08_TABLELOG_MAX<=12)) \
        HUFv08_DECODE_SYMBOLX2_0(ptr, DStreamPtr)

#define HUFv08_DECODE_SYMBOLX2_2(ptr, DStreamPtr) \
    if (MEM_64bits()) \
        HUFv08_DECODE_SYMBOLX2_0(ptr, DStreamPtr)

static inline size_t HUFv08_decodeStreamX2(BYTE* p, BIT_DStream_t* const bitDPtr, BYTE* const pEnd, const HUFv08_DEltX2* const dt, const U32 dtLog)
{
    BYTE* const pStart = p;

    /* up to 4 symbols at a time */
    while ((BIT_reloadDStream(bitDPtr) == BIT_DStream_unfinished) && (p <= pEnd-4)) {
        HUFv08_DECODE_SYMBOLX2_2(p, bitDPtr);
        HUFv08_DECODE_SYMBOLX2_1(p, bitDPtr);
        HUFv08_DECODE_SYMBOLX2_2(p, bitDPtr);
        HUFv08_DECODE_SYMBOLX2_0(p, bitDPtr);
    }

    /* closer to the end */
    while ((BIT_reloadDStream(bitDPtr) == BIT_DStream_unfinished) && (p < pEnd))
        HUFv08_DECODE_SYMBOLX2_0(p, bitDPtr);

    /* no more data to retrieve from bitstream, hence no need to reload */
    while (p < pEnd)
        HUFv08_DECODE_SYMBOLX2_0(p, bitDPtr);

    return pEnd-pStart;
}

static size_t HUFv08_decompress1X2_usingDTable_internal(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFv08_DTable* DTable)
{
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + dstSize;
    const void* dtPtr = DTable + 1;
    const HUFv08_DEltX2* const dt = (const HUFv08_DEltX2*)dtPtr;
    BIT_DStream_t bitD;
    DTableDesc const dtd = HUFv08_getDTableDesc(DTable);
    U32 const dtLog = dtd.tableLog;

    { size_t const errorCode = BIT_initDStream(&bitD, cSrc, cSrcSize);
      if (HUFv08_isError(errorCode)) return errorCode; }

    HUFv08_decodeStreamX2(op, &bitD, oend, dt, dtLog);

    /* check */
    if (!BIT_endOfDStream(&bitD)) return ERROR(corruption_detected);

    return dstSize;
}

size_t HUFv08_decompress1X2_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFv08_DTable* DTable)
{
    DTableDesc dtd = HUFv08_getDTableDesc(DTable);
    if (dtd.tableType != 0) return ERROR(GENERIC);
    return HUFv08_decompress1X2_usingDTable_internal(dst, dstSize, cSrc, cSrcSize, DTable);
}

size_t HUFv08_decompress1X2_DCtx (HUFv08_DTable* DCtx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    const BYTE* ip = (const BYTE*) cSrc;

    size_t const hSize = HUFv08_readDTableX2 (DCtx, cSrc, cSrcSize);
    if (HUFv08_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize; cSrcSize -= hSize;

    return HUFv08_decompress1X2_usingDTable_internal (dst, dstSize, ip, cSrcSize, DCtx);
}

size_t HUFv08_decompress1X2 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    HUFv08_CREATE_STATIC_DTABLEX2(DTable, HUFv08_TABLELOG_MAX);
    return HUFv08_decompress1X2_DCtx (DTable, dst, dstSize, cSrc, cSrcSize);
}


static size_t HUFv08_decompress4X2_usingDTable_internal(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFv08_DTable* DTable)
{
    /* Check */
    if (cSrcSize < 10) return ERROR(corruption_detected);  /* strict minimum : jump table + 1 byte per stream */

    {   const BYTE* const istart = (const BYTE*) cSrc;
        BYTE* const ostart = (BYTE*) dst;
        BYTE* const oend = ostart + dstSize;
        const void* const dtPtr = DTable + 1;
        const HUFv08_DEltX2* const dt = (const HUFv08_DEltX2*)dtPtr;

        /* Init */
        BIT_DStream_t bitD1;
        BIT_DStream_t bitD2;
        BIT_DStream_t bitD3;
        BIT_DStream_t bitD4;
        size_t const length1 = MEM_readLE16(istart);
        size_t const length2 = MEM_readLE16(istart+2);
        size_t const length3 = MEM_readLE16(istart+4);
        size_t const length4 = cSrcSize - (length1 + length2 + length3 + 6);
        const BYTE* const istart1 = istart + 6;  /* jumpTable */
        const BYTE* const istart2 = istart1 + length1;
        const BYTE* const istart3 = istart2 + length2;
        const BYTE* const istart4 = istart3 + length3;
        const size_t segmentSize = (dstSize+3) / 4;
        BYTE* const opStart2 = ostart + segmentSize;
        BYTE* const opStart3 = opStart2 + segmentSize;
        BYTE* const opStart4 = opStart3 + segmentSize;
        BYTE* op1 = ostart;
        BYTE* op2 = opStart2;
        BYTE* op3 = opStart3;
        BYTE* op4 = opStart4;
        U32 endSignal;
        DTableDesc const dtd = HUFv08_getDTableDesc(DTable);
        U32 const dtLog = dtd.tableLog;

        if (length4 > cSrcSize) return ERROR(corruption_detected);   /* overflow */
        { size_t const errorCode = BIT_initDStream(&bitD1, istart1, length1);
          if (HUFv08_isError(errorCode)) return errorCode; }
        { size_t const errorCode = BIT_initDStream(&bitD2, istart2, length2);
          if (HUFv08_isError(errorCode)) return errorCode; }
        { size_t const errorCode = BIT_initDStream(&bitD3, istart3, length3);
          if (HUFv08_isError(errorCode)) return errorCode; }
        { size_t const errorCode = BIT_initDStream(&bitD4, istart4, length4);
          if (HUFv08_isError(errorCode)) return errorCode; }

        /* 16-32 symbols per loop (4-8 symbols per stream) */
        endSignal = BIT_reloadDStream(&bitD1) | BIT_reloadDStream(&bitD2) | BIT_reloadDStream(&bitD3) | BIT_reloadDStream(&bitD4);
        for ( ; (endSignal==BIT_DStream_unfinished) && (op4<(oend-7)) ; ) {
            HUFv08_DECODE_SYMBOLX2_2(op1, &bitD1);
            HUFv08_DECODE_SYMBOLX2_2(op2, &bitD2);
            HUFv08_DECODE_SYMBOLX2_2(op3, &bitD3);
            HUFv08_DECODE_SYMBOLX2_2(op4, &bitD4);
            HUFv08_DECODE_SYMBOLX2_1(op1, &bitD1);
            HUFv08_DECODE_SYMBOLX2_1(op2, &bitD2);
            HUFv08_DECODE_SYMBOLX2_1(op3, &bitD3);
            HUFv08_DECODE_SYMBOLX2_1(op4, &bitD4);
            HUFv08_DECODE_SYMBOLX2_2(op1, &bitD1);
            HUFv08_DECODE_SYMBOLX2_2(op2, &bitD2);
            HUFv08_DECODE_SYMBOLX2_2(op3, &bitD3);
            HUFv08_DECODE_SYMBOLX2_2(op4, &bitD4);
            HUFv08_DECODE_SYMBOLX2_0(op1, &bitD1);
            HUFv08_DECODE_SYMBOLX2_0(op2, &bitD2);
            HUFv08_DECODE_SYMBOLX2_0(op3, &bitD3);
            HUFv08_DECODE_SYMBOLX2_0(op4, &bitD4);
            endSignal = BIT_reloadDStream(&bitD1) | BIT_reloadDStream(&bitD2) | BIT_reloadDStream(&bitD3) | BIT_reloadDStream(&bitD4);
        }

        /* check corruption */
        if (op1 > opStart2) return ERROR(corruption_detected);
        if (op2 > opStart3) return ERROR(corruption_detected);
        if (op3 > opStart4) return ERROR(corruption_detected);
        /* note : op4 supposed already verified within main loop */

        /* finish bitStreams one by one */
        HUFv08_decodeStreamX2(op1, &bitD1, opStart2, dt, dtLog);
        HUFv08_decodeStreamX2(op2, &bitD2, opStart3, dt, dtLog);
        HUFv08_decodeStreamX2(op3, &bitD3, opStart4, dt, dtLog);
        HUFv08_decodeStreamX2(op4, &bitD4, oend,     dt, dtLog);

        /* check */
        endSignal = BIT_endOfDStream(&bitD1) & BIT_endOfDStream(&bitD2) & BIT_endOfDStream(&bitD3) & BIT_endOfDStream(&bitD4);
        if (!endSignal) return ERROR(corruption_detected);

        /* decoded size */
        return dstSize;
    }
}


size_t HUFv08_decompress4X2_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFv08_DTable* DTable)
{
    DTableDesc dtd = HUFv08_getDTableDesc(DTable);
    if (dtd.tableType != 0) return ERROR(GENERIC);
    return HUFv08_decompress4X2_usingDTable_internal(dst, dstSize, cSrc, cSrcSize, DTable);
}


size_t HUFv08_decompress4X2_DCtx (HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    const BYTE* ip = (const BYTE*) cSrc;

    size_t const hSize = HUFv08_readDTableX2 (dctx, cSrc, cSrcSize);
    if (HUFv08_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize; cSrcSize -= hSize;

    return HUFv08_decompress4X2_usingDTable_internal (dst, dstSize, ip, cSrcSize, dctx);
}

size_t HUFv08_decompress4X2 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    HUFv08_CREATE_STATIC_DTABLEX2(DTable, HUFv08_TABLELOG_MAX);
    return HUFv08_decompress4X2_DCtx(DTable, dst, dstSize, cSrc, cSrcSize);
}


/* *************************/
/* double-symbols decoding */
/* *************************/
typedef struct { U16 sequence; BYTE nbBits; BYTE length; } HUFv08_DEltX4;  /* double-symbols decoding */

typedef struct { BYTE symbol; BYTE weight; } sortedSymbol_t;

static void HUFv08_fillDTableX4Level2(HUFv08_DEltX4* DTable, U32 sizeLog, const U32 consumed,
                           const U32* rankValOrigin, const int minWeight,
                           const sortedSymbol_t* sortedSymbols, const U32 sortedListSize,
                           U32 nbBitsBaseline, U16 baseSeq)
{
    HUFv08_DEltX4 DElt;
    U32 rankVal[HUFv08_TABLELOG_ABSOLUTEMAX + 1];

    /* get pre-calculated rankVal */
    memcpy(rankVal, rankValOrigin, sizeof(rankVal));

    /* fill skipped values */
    if (minWeight>1) {
        U32 i, skipSize = rankVal[minWeight];
        MEM_writeLE16(&(DElt.sequence), baseSeq);
        DElt.nbBits   = (BYTE)(consumed);
        DElt.length   = 1;
        for (i = 0; i < skipSize; i++)
            DTable[i] = DElt;
    }

    /* fill DTable */
    { U32 s; for (s=0; s<sortedListSize; s++) {   /* note : sortedSymbols already skipped */
        const U32 symbol = sortedSymbols[s].symbol;
        const U32 weight = sortedSymbols[s].weight;
        const U32 nbBits = nbBitsBaseline - weight;
        const U32 length = 1 << (sizeLog-nbBits);
        const U32 start = rankVal[weight];
        U32 i = start;
        const U32 end = start + length;

        MEM_writeLE16(&(DElt.sequence), (U16)(baseSeq + (symbol << 8)));
        DElt.nbBits = (BYTE)(nbBits + consumed);
        DElt.length = 2;
        do { DTable[i++] = DElt; } while (i<end);   /* since length >= 1 */

        rankVal[weight] += length;
    }}
}

typedef U32 rankVal_t[HUFv08_TABLELOG_ABSOLUTEMAX][HUFv08_TABLELOG_ABSOLUTEMAX + 1];

static void HUFv08_fillDTableX4(HUFv08_DEltX4* DTable, const U32 targetLog,
                           const sortedSymbol_t* sortedList, const U32 sortedListSize,
                           const U32* rankStart, rankVal_t rankValOrigin, const U32 maxWeight,
                           const U32 nbBitsBaseline)
{
    U32 rankVal[HUFv08_TABLELOG_ABSOLUTEMAX + 1];
    const int scaleLog = nbBitsBaseline - targetLog;   /* note : targetLog >= srcLog, hence scaleLog <= 1 */
    const U32 minBits  = nbBitsBaseline - maxWeight;
    U32 s;

    memcpy(rankVal, rankValOrigin, sizeof(rankVal));

    /* fill DTable */
    for (s=0; s<sortedListSize; s++) {
        const U16 symbol = sortedList[s].symbol;
        const U32 weight = sortedList[s].weight;
        const U32 nbBits = nbBitsBaseline - weight;
        const U32 start = rankVal[weight];
        const U32 length = 1 << (targetLog-nbBits);

        if (targetLog-nbBits >= minBits) {   /* enough room for a second symbol */
            U32 sortedRank;
            int minWeight = nbBits + scaleLog;
            if (minWeight < 1) minWeight = 1;
            sortedRank = rankStart[minWeight];
            HUFv08_fillDTableX4Level2(DTable+start, targetLog-nbBits, nbBits,
                           rankValOrigin[nbBits], minWeight,
                           sortedList+sortedRank, sortedListSize-sortedRank,
                           nbBitsBaseline, symbol);
        } else {
            HUFv08_DEltX4 DElt;
            MEM_writeLE16(&(DElt.sequence), symbol);
            DElt.nbBits = (BYTE)(nbBits);
            DElt.length = 1;
            {   U32 u;
                const U32 end = start + length;
                for (u = start; u < end; u++) DTable[u] = DElt;
        }   }
        rankVal[weight] += length;
    }
}

size_t HUFv08_readDTableX4 (HUFv08_DTable* DTable, const void* src, size_t srcSize)
{
    BYTE weightList[HUFv08_SYMBOLVALUE_MAX + 1];
    sortedSymbol_t sortedSymbol[HUFv08_SYMBOLVALUE_MAX + 1];
    U32 rankStats[HUFv08_TABLELOG_ABSOLUTEMAX + 1] = { 0 };
    U32 rankStart0[HUFv08_TABLELOG_ABSOLUTEMAX + 2] = { 0 };
    U32* const rankStart = rankStart0+1;
    rankVal_t rankVal;
    U32 tableLog, maxW, sizeOfSort, nbSymbols;
    DTableDesc dtd = HUFv08_getDTableDesc(DTable);
    U32 const maxTableLog = dtd.maxTableLog;
    size_t iSize;
    void* dtPtr = DTable+1;   /* force compiler to avoid strict-aliasing */
    HUFv08_DEltX4* const dt = (HUFv08_DEltX4*)dtPtr;

    HUFv08_STATIC_ASSERT(sizeof(HUFv08_DEltX4) == sizeof(HUFv08_DTable));   /* if compilation fails here, assertion is false */
    if (maxTableLog > HUFv08_TABLELOG_ABSOLUTEMAX) return ERROR(tableLog_tooLarge);
    //memset(weightList, 0, sizeof(weightList));   /* is not necessary, even though some analyzer complain ... */

    iSize = HUFv08_readStats(weightList, HUFv08_SYMBOLVALUE_MAX + 1, rankStats, &nbSymbols, &tableLog, src, srcSize);
    if (HUFv08_isError(iSize)) return iSize;

    /* check result */
    if (tableLog > maxTableLog) return ERROR(tableLog_tooLarge);   /* DTable can't fit code depth */

    /* find maxWeight */
    for (maxW = tableLog; rankStats[maxW]==0; maxW--) {}  /* necessarily finds a solution before 0 */

    /* Get start index of each weight */
    {   U32 w, nextRankStart = 0;
        for (w=1; w<maxW+1; w++) {
            U32 current = nextRankStart;
            nextRankStart += rankStats[w];
            rankStart[w] = current;
        }
        rankStart[0] = nextRankStart;   /* put all 0w symbols at the end of sorted list*/
        sizeOfSort = nextRankStart;
    }

    /* sort symbols by weight */
    {   U32 s;
        for (s=0; s<nbSymbols; s++) {
            U32 const w = weightList[s];
            U32 const r = rankStart[w]++;
            sortedSymbol[r].symbol = (BYTE)s;
            sortedSymbol[r].weight = (BYTE)w;
        }
        rankStart[0] = 0;   /* forget 0w symbols; this is beginning of weight(1) */
    }

    /* Build rankVal */
    {   U32* const rankVal0 = rankVal[0];
        {   int const rescale = (maxTableLog-tableLog) - 1;   /* tableLog <= maxTableLog */
            U32 nextRankVal = 0;
            U32 w;
            for (w=1; w<maxW+1; w++) {
                U32 current = nextRankVal;
                nextRankVal += rankStats[w] << (w+rescale);
                rankVal0[w] = current;
        }   }
        {   U32 const minBits = tableLog+1 - maxW;
            U32 consumed;
            for (consumed = minBits; consumed < maxTableLog - minBits + 1; consumed++) {
                U32* const rankValPtr = rankVal[consumed];
                U32 w;
                for (w = 1; w < maxW+1; w++) {
                    rankValPtr[w] = rankVal0[w] >> consumed;
    }   }   }   }

    HUFv08_fillDTableX4(dt, maxTableLog,
                   sortedSymbol, sizeOfSort,
                   rankStart0, rankVal, maxW,
                   tableLog+1);

    dtd.tableLog = (BYTE)maxTableLog;
    dtd.tableType = 1;
    memcpy(DTable, &dtd, sizeof(dtd));
    return iSize;
}


static U32 HUFv08_decodeSymbolX4(void* op, BIT_DStream_t* DStream, const HUFv08_DEltX4* dt, const U32 dtLog)
{
    const size_t val = BIT_lookBitsFast(DStream, dtLog);   /* note : dtLog >= 1 */
    memcpy(op, dt+val, 2);
    BIT_skipBits(DStream, dt[val].nbBits);
    return dt[val].length;
}

static U32 HUFv08_decodeLastSymbolX4(void* op, BIT_DStream_t* DStream, const HUFv08_DEltX4* dt, const U32 dtLog)
{
    const size_t val = BIT_lookBitsFast(DStream, dtLog);   /* note : dtLog >= 1 */
    memcpy(op, dt+val, 1);
    if (dt[val].length==1) BIT_skipBits(DStream, dt[val].nbBits);
    else {
        if (DStream->bitsConsumed < (sizeof(DStream->bitContainer)*8)) {
            BIT_skipBits(DStream, dt[val].nbBits);
            if (DStream->bitsConsumed > (sizeof(DStream->bitContainer)*8))
                DStream->bitsConsumed = (sizeof(DStream->bitContainer)*8);   /* ugly hack; works only because it's the last symbol. Note : can't easily extract nbBits from just this symbol */
    }   }
    return 1;
}


#define HUFv08_DECODE_SYMBOLX4_0(ptr, DStreamPtr) \
    ptr += HUFv08_decodeSymbolX4(ptr, DStreamPtr, dt, dtLog)

#define HUFv08_DECODE_SYMBOLX4_1(ptr, DStreamPtr) \
    if (MEM_64bits() || (HUFv08_TABLELOG_MAX<=12)) \
        ptr += HUFv08_decodeSymbolX4(ptr, DStreamPtr, dt, dtLog)

#define HUFv08_DECODE_SYMBOLX4_2(ptr, DStreamPtr) \
    if (MEM_64bits()) \
        ptr += HUFv08_decodeSymbolX4(ptr, DStreamPtr, dt, dtLog)

static inline size_t HUFv08_decodeStreamX4(BYTE* p, BIT_DStream_t* bitDPtr, BYTE* const pEnd, const HUFv08_DEltX4* const dt, const U32 dtLog)
{
    BYTE* const pStart = p;

    /* up to 8 symbols at a time */
    while ((BIT_reloadDStream(bitDPtr) == BIT_DStream_unfinished) && (p < pEnd-7)) {
        HUFv08_DECODE_SYMBOLX4_2(p, bitDPtr);
        HUFv08_DECODE_SYMBOLX4_1(p, bitDPtr);
        HUFv08_DECODE_SYMBOLX4_2(p, bitDPtr);
        HUFv08_DECODE_SYMBOLX4_0(p, bitDPtr);
    }

    /* closer to end : up to 2 symbols at a time */
    while ((BIT_reloadDStream(bitDPtr) == BIT_DStream_unfinished) && (p <= pEnd-2))
        HUFv08_DECODE_SYMBOLX4_0(p, bitDPtr);

    while (p <= pEnd-2)
        HUFv08_DECODE_SYMBOLX4_0(p, bitDPtr);   /* no need to reload : reached the end of DStream */

    if (p < pEnd)
        p += HUFv08_decodeLastSymbolX4(p, bitDPtr, dt, dtLog);

    return p-pStart;
}


static size_t HUFv08_decompress1X4_usingDTable_internal(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFv08_DTable* DTable)
{
    BIT_DStream_t bitD;

    /* Init */
    {   size_t const errorCode = BIT_initDStream(&bitD, cSrc, cSrcSize);
        if (HUFv08_isError(errorCode)) return errorCode;
    }

    /* decode */
    {   BYTE* const ostart = (BYTE*) dst;
        BYTE* const oend = ostart + dstSize;
        const void* const dtPtr = DTable+1;   /* force compiler to not use strict-aliasing */
        const HUFv08_DEltX4* const dt = (const HUFv08_DEltX4*)dtPtr;
        DTableDesc const dtd = HUFv08_getDTableDesc(DTable);
        HUFv08_decodeStreamX4(ostart, &bitD, oend, dt, dtd.tableLog);
    }

    /* check */
    if (!BIT_endOfDStream(&bitD)) return ERROR(corruption_detected);

    /* decoded size */
    return dstSize;
}

size_t HUFv08_decompress1X4_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFv08_DTable* DTable)
{
    DTableDesc dtd = HUFv08_getDTableDesc(DTable);
    if (dtd.tableType != 1) return ERROR(GENERIC);
    return HUFv08_decompress1X4_usingDTable_internal(dst, dstSize, cSrc, cSrcSize, DTable);
}

size_t HUFv08_decompress1X4_DCtx (HUFv08_DTable* DCtx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    const BYTE* ip = (const BYTE*) cSrc;

    size_t const hSize = HUFv08_readDTableX4 (DCtx, cSrc, cSrcSize);
    if (HUFv08_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize; cSrcSize -= hSize;

    return HUFv08_decompress1X4_usingDTable_internal (dst, dstSize, ip, cSrcSize, DCtx);
}

size_t HUFv08_decompress1X4 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    HUFv08_CREATE_STATIC_DTABLEX4(DTable, HUFv08_TABLELOG_MAX);
    return HUFv08_decompress1X4_DCtx(DTable, dst, dstSize, cSrc, cSrcSize);
}

static size_t HUFv08_decompress4X4_usingDTable_internal(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFv08_DTable* DTable)
{
    if (cSrcSize < 10) return ERROR(corruption_detected);   /* strict minimum : jump table + 1 byte per stream */

    {   const BYTE* const istart = (const BYTE*) cSrc;
        BYTE* const ostart = (BYTE*) dst;
        BYTE* const oend = ostart + dstSize;
        const void* const dtPtr = DTable+1;
        const HUFv08_DEltX4* const dt = (const HUFv08_DEltX4*)dtPtr;

        /* Init */
        BIT_DStream_t bitD1;
        BIT_DStream_t bitD2;
        BIT_DStream_t bitD3;
        BIT_DStream_t bitD4;
        size_t const length1 = MEM_readLE16(istart);
        size_t const length2 = MEM_readLE16(istart+2);
        size_t const length3 = MEM_readLE16(istart+4);
        size_t const length4 = cSrcSize - (length1 + length2 + length3 + 6);
        const BYTE* const istart1 = istart + 6;  /* jumpTable */
        const BYTE* const istart2 = istart1 + length1;
        const BYTE* const istart3 = istart2 + length2;
        const BYTE* const istart4 = istart3 + length3;
        size_t const segmentSize = (dstSize+3) / 4;
        BYTE* const opStart2 = ostart + segmentSize;
        BYTE* const opStart3 = opStart2 + segmentSize;
        BYTE* const opStart4 = opStart3 + segmentSize;
        BYTE* op1 = ostart;
        BYTE* op2 = opStart2;
        BYTE* op3 = opStart3;
        BYTE* op4 = opStart4;
        U32 endSignal;
        DTableDesc const dtd = HUFv08_getDTableDesc(DTable);
        U32 const dtLog = dtd.tableLog;

        if (length4 > cSrcSize) return ERROR(corruption_detected);   /* overflow */
        { size_t const errorCode = BIT_initDStream(&bitD1, istart1, length1);
          if (HUFv08_isError(errorCode)) return errorCode; }
        { size_t const errorCode = BIT_initDStream(&bitD2, istart2, length2);
          if (HUFv08_isError(errorCode)) return errorCode; }
        { size_t const errorCode = BIT_initDStream(&bitD3, istart3, length3);
          if (HUFv08_isError(errorCode)) return errorCode; }
        { size_t const errorCode = BIT_initDStream(&bitD4, istart4, length4);
          if (HUFv08_isError(errorCode)) return errorCode; }

        /* 16-32 symbols per loop (4-8 symbols per stream) */
        endSignal = BIT_reloadDStream(&bitD1) | BIT_reloadDStream(&bitD2) | BIT_reloadDStream(&bitD3) | BIT_reloadDStream(&bitD4);
        for ( ; (endSignal==BIT_DStream_unfinished) && (op4<(oend-7)) ; ) {
            HUFv08_DECODE_SYMBOLX4_2(op1, &bitD1);
            HUFv08_DECODE_SYMBOLX4_2(op2, &bitD2);
            HUFv08_DECODE_SYMBOLX4_2(op3, &bitD3);
            HUFv08_DECODE_SYMBOLX4_2(op4, &bitD4);
            HUFv08_DECODE_SYMBOLX4_1(op1, &bitD1);
            HUFv08_DECODE_SYMBOLX4_1(op2, &bitD2);
            HUFv08_DECODE_SYMBOLX4_1(op3, &bitD3);
            HUFv08_DECODE_SYMBOLX4_1(op4, &bitD4);
            HUFv08_DECODE_SYMBOLX4_2(op1, &bitD1);
            HUFv08_DECODE_SYMBOLX4_2(op2, &bitD2);
            HUFv08_DECODE_SYMBOLX4_2(op3, &bitD3);
            HUFv08_DECODE_SYMBOLX4_2(op4, &bitD4);
            HUFv08_DECODE_SYMBOLX4_0(op1, &bitD1);
            HUFv08_DECODE_SYMBOLX4_0(op2, &bitD2);
            HUFv08_DECODE_SYMBOLX4_0(op3, &bitD3);
            HUFv08_DECODE_SYMBOLX4_0(op4, &bitD4);

            endSignal = BIT_reloadDStream(&bitD1) | BIT_reloadDStream(&bitD2) | BIT_reloadDStream(&bitD3) | BIT_reloadDStream(&bitD4);
        }

        /* check corruption */
        if (op1 > opStart2) return ERROR(corruption_detected);
        if (op2 > opStart3) return ERROR(corruption_detected);
        if (op3 > opStart4) return ERROR(corruption_detected);
        /* note : op4 supposed already verified within main loop */

        /* finish bitStreams one by one */
        HUFv08_decodeStreamX4(op1, &bitD1, opStart2, dt, dtLog);
        HUFv08_decodeStreamX4(op2, &bitD2, opStart3, dt, dtLog);
        HUFv08_decodeStreamX4(op3, &bitD3, opStart4, dt, dtLog);
        HUFv08_decodeStreamX4(op4, &bitD4, oend,     dt, dtLog);

        /* check */
        { U32 const endCheck = BIT_endOfDStream(&bitD1) & BIT_endOfDStream(&bitD2) & BIT_endOfDStream(&bitD3) & BIT_endOfDStream(&bitD4);
          if (!endCheck) return ERROR(corruption_detected); }

        /* decoded size */
        return dstSize;
    }
}


size_t HUFv08_decompress4X4_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFv08_DTable* DTable)
{
    DTableDesc dtd = HUFv08_getDTableDesc(DTable);
    if (dtd.tableType != 1) return ERROR(GENERIC);
    return HUFv08_decompress4X4_usingDTable_internal(dst, dstSize, cSrc, cSrcSize, DTable);
}


size_t HUFv08_decompress4X4_DCtx (HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    const BYTE* ip = (const BYTE*) cSrc;

    size_t hSize = HUFv08_readDTableX4 (dctx, cSrc, cSrcSize);
    if (HUFv08_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize; cSrcSize -= hSize;

    return HUFv08_decompress4X4_usingDTable_internal(dst, dstSize, ip, cSrcSize, dctx);
}

size_t HUFv08_decompress4X4 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    HUFv08_CREATE_STATIC_DTABLEX4(DTable, HUFv08_TABLELOG_MAX);
    return HUFv08_decompress4X4_DCtx(DTable, dst, dstSize, cSrc, cSrcSize);
}


/* ********************************/
/* Generic decompression selector */
/* ********************************/

size_t HUFv08_decompress1X_usingDTable(void* dst, size_t maxDstSize,
                                    const void* cSrc, size_t cSrcSize,
                                    const HUFv08_DTable* DTable)
{
    DTableDesc const dtd = HUFv08_getDTableDesc(DTable);
    return dtd.tableType ? HUFv08_decompress1X4_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable) :
                           HUFv08_decompress1X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable);
}

size_t HUFv08_decompress4X_usingDTable(void* dst, size_t maxDstSize,
                                    const void* cSrc, size_t cSrcSize,
                                    const HUFv08_DTable* DTable)
{
    DTableDesc const dtd = HUFv08_getDTableDesc(DTable);
    return dtd.tableType ? HUFv08_decompress4X4_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable) :
                           HUFv08_decompress4X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable);
}


typedef struct { U32 tableTime; U32 decode256Time; } algo_time_t;
static const algo_time_t algoTime[16 /* Quantization */][3 /* single, double, quad */] =
{
    /* single, double, quad */
    {{0,0}, {1,1}, {2,2}},  /* Q==0 : impossible */
    {{0,0}, {1,1}, {2,2}},  /* Q==1 : impossible */
    {{  38,130}, {1313, 74}, {2151, 38}},   /* Q == 2 : 12-18% */
    {{ 448,128}, {1353, 74}, {2238, 41}},   /* Q == 3 : 18-25% */
    {{ 556,128}, {1353, 74}, {2238, 47}},   /* Q == 4 : 25-32% */
    {{ 714,128}, {1418, 74}, {2436, 53}},   /* Q == 5 : 32-38% */
    {{ 883,128}, {1437, 74}, {2464, 61}},   /* Q == 6 : 38-44% */
    {{ 897,128}, {1515, 75}, {2622, 68}},   /* Q == 7 : 44-50% */
    {{ 926,128}, {1613, 75}, {2730, 75}},   /* Q == 8 : 50-56% */
    {{ 947,128}, {1729, 77}, {3359, 77}},   /* Q == 9 : 56-62% */
    {{1107,128}, {2083, 81}, {4006, 84}},   /* Q ==10 : 62-69% */
    {{1177,128}, {2379, 87}, {4785, 88}},   /* Q ==11 : 69-75% */
    {{1242,128}, {2415, 93}, {5155, 84}},   /* Q ==12 : 75-81% */
    {{1349,128}, {2644,106}, {5260,106}},   /* Q ==13 : 81-87% */
    {{1455,128}, {2422,124}, {4174,124}},   /* Q ==14 : 87-93% */
    {{ 722,128}, {1891,145}, {1936,146}},   /* Q ==15 : 93-99% */
};

/** HUFv08_selectDecoder() :
*   Tells which decoder is likely to decode faster,
*   based on a set of pre-determined metrics.
*   @return : 0==HUFv08_decompress4X2, 1==HUFv08_decompress4X4 .
*   Assumption : 0 < cSrcSize < dstSize <= 128 KB */
U32 HUFv08_selectDecoder (size_t dstSize, size_t cSrcSize)
{
    /* decoder timing evaluation */
    U32 const Q = (U32)(cSrcSize * 16 / dstSize);   /* Q < 16 since dstSize > cSrcSize */
    U32 const D256 = (U32)(dstSize >> 8);
    U32 const DTime0 = algoTime[Q][0].tableTime + (algoTime[Q][0].decode256Time * D256);
    U32 DTime1 = algoTime[Q][1].tableTime + (algoTime[Q][1].decode256Time * D256);
    DTime1 += DTime1 >> 3;  /* advantage to algorithm using less memory, for cache eviction */

    return DTime1 < DTime0;
}


typedef size_t (*decompressionAlgo)(void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);

size_t HUFv08_decompress (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    static const decompressionAlgo decompress[2] = { HUFv08_decompress4X2, HUFv08_decompress4X4 };

    /* validation checks */
    if (dstSize == 0) return ERROR(dstSize_tooSmall);
    if (cSrcSize > dstSize) return ERROR(corruption_detected);   /* invalid */
    if (cSrcSize == dstSize) { memcpy(dst, cSrc, dstSize); return dstSize; }   /* not compressed */
    if (cSrcSize == 1) { memset(dst, *(const BYTE*)cSrc, dstSize); return dstSize; }   /* RLE */

    {   U32 const algoNb = HUFv08_selectDecoder(dstSize, cSrcSize);
        return decompress[algoNb](dst, dstSize, cSrc, cSrcSize);
    }

    //return HUFv08_decompress4X2(dst, dstSize, cSrc, cSrcSize);   /* multi-streams single-symbol decoding */
    //return HUFv08_decompress4X4(dst, dstSize, cSrc, cSrcSize);   /* multi-streams double-symbols decoding */
}

size_t HUFv08_decompress4X_DCtx (HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    /* validation checks */
    if (dstSize == 0) return ERROR(dstSize_tooSmall);
    if (cSrcSize > dstSize) return ERROR(corruption_detected);   /* invalid */
    if (cSrcSize == dstSize) { memcpy(dst, cSrc, dstSize); return dstSize; }   /* not compressed */
    if (cSrcSize == 1) { memset(dst, *(const BYTE*)cSrc, dstSize); return dstSize; }   /* RLE */

    {   U32 const algoNb = HUFv08_selectDecoder(dstSize, cSrcSize);
        return algoNb ? HUFv08_decompress4X4_DCtx(dctx, dst, dstSize, cSrc, cSrcSize) :
                        HUFv08_decompress4X2_DCtx(dctx, dst, dstSize, cSrc, cSrcSize) ;
    }
}

size_t HUFv08_decompress4X_hufOnly (HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    /* validation checks */
    if (dstSize == 0) return ERROR(dstSize_tooSmall);
    if ((cSrcSize >= dstSize) || (cSrcSize <= 1)) return ERROR(corruption_detected);   /* invalid */

    {   U32 const algoNb = HUFv08_selectDecoder(dstSize, cSrcSize);
        return algoNb ? HUFv08_decompress4X4_DCtx(dctx, dst, dstSize, cSrc, cSrcSize) :
                        HUFv08_decompress4X2_DCtx(dctx, dst, dstSize, cSrc, cSrcSize) ;
    }
}

size_t HUFv08_decompress1X_DCtx (HUFv08_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    /* validation checks */
    if (dstSize == 0) return ERROR(dstSize_tooSmall);
    if (cSrcSize > dstSize) return ERROR(corruption_detected);   /* invalid */
    if (cSrcSize == dstSize) { memcpy(dst, cSrc, dstSize); return dstSize; }   /* not compressed */
    if (cSrcSize == 1) { memset(dst, *(const BYTE*)cSrc, dstSize); return dstSize; }   /* RLE */

    {   U32 const algoNb = HUFv08_selectDecoder(dstSize, cSrcSize);
        return algoNb ? HUFv08_decompress1X4_DCtx(dctx, dst, dstSize, cSrc, cSrcSize) :
                        HUFv08_decompress1X2_DCtx(dctx, dst, dstSize, cSrc, cSrcSize) ;
    }
}
/*
    zstd_internal - common functions to include
    Header File for include
    Copyright (C) 2014-2016, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - zstd homepage : https://www.zstd.net
*/
#ifndef ZSTDv08_CCOMMON_H_MODULE
#define ZSTDv08_CCOMMON_H_MODULE


/*-*************************************
*  Common macros
***************************************/
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))


/*-*************************************
*  Common constants
***************************************/
#define ZSTDv08_OPT_DEBUG 0     /* 3 = compression stats;  5 = check encoded sequences;  9 = full logs */
#if defined(ZSTDv08_OPT_DEBUG) && ZSTDv08_OPT_DEBUG>=9
    #include <stdio.h>
    #include <stdlib.h>
    #define ZSTDv08_LOG_PARSER(...) printf(__VA_ARGS__)
    #define ZSTDv08_LOG_ENCODE(...) printf(__VA_ARGS__)
    #define ZSTDv08_LOG_BLOCK(...) printf(__VA_ARGS__)
#else
    #define ZSTDv08_LOG_PARSER(...)
    #define ZSTDv08_LOG_ENCODE(...)
    #define ZSTDv08_LOG_BLOCK(...)
#endif

#define ZSTDv08_OPT_NUM    (1<<12)
#define ZSTDv08_DICT_MAGIC  0xEC30A437   /* v0.7+ */

#define ZSTDv08_REP_NUM    3                 /* number of repcodes */
#define ZSTDv08_REP_CHECK  (ZSTDv08_REP_NUM-0)  /* number of repcodes to check by the optimal parser */
#define ZSTDv08_REP_MOVE   (ZSTDv08_REP_NUM-1)
static const U32 repStartValue[ZSTDv08_REP_NUM] = { 1, 4, 8 };

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define BIT7 128
#define BIT6  64
#define BIT5  32
#define BIT4  16
#define BIT1   2
#define BIT0   1

#define ZSTDv08_WINDOWLOG_ABSOLUTEMIN 10
static const size_t ZSTDv08_fcs_fieldSize[4] = { 0, 2, 4, 8 };
static const size_t ZSTDv08_did_fieldSize[4] = { 0, 1, 2, 4 };

#define ZSTDv08_BLOCKHEADERSIZE 3   /* C standard doesn't allow `static const` variable to be init using another `static const` variable */
static const size_t ZSTDv08_blockHeaderSize = ZSTDv08_BLOCKHEADERSIZE;
typedef enum { bt_raw, bt_rle, bt_compressed, bt_reserved } blockType_e;

#define MIN_SEQUENCES_SIZE 1 /* nbSeq==0 */
#define MIN_CBLOCK_SIZE (1 /*litCSize*/ + 1 /* RLE or RAW */ + MIN_SEQUENCES_SIZE /* nbSeq==0 */)   /* for a non-null block */

#define HufLog 12
typedef enum { set_basic, set_rle, set_compressed, set_repeat } symbolEncodingType_e;

#define LONGNBSEQ 0x7F00

#define MINMATCH 3
#define EQUAL_READ32 4

#define Litbits  8
#define MaxLit ((1<<Litbits) - 1)
#define MaxML  52
#define MaxLL  35
#define MaxOff 28
#define MaxSeq MAX(MaxLL, MaxML)   /* Assumption : MaxOff < MaxLL,MaxML */
#define MLFSELog    9
#define LLFSELog    9
#define OffFSELog   8

static const U32 LL_bits[MaxLL+1] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      1, 1, 1, 1, 2, 2, 3, 3, 4, 6, 7, 8, 9,10,11,12,
                                     13,14,15,16 };
static const S16 LL_defaultNorm[MaxLL+1] = { 4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1,
                                             2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1,
                                            -1,-1,-1,-1 };
static const U32 LL_defaultNormLog = 6;

static const U32 ML_bits[MaxML+1] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 7, 8, 9,10,11,
                                     12,13,14,15,16 };
static const S16 ML_defaultNorm[MaxML+1] = { 1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
                                             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,-1,-1,
                                            -1,-1,-1,-1,-1 };
static const U32 ML_defaultNormLog = 6;

static const S16 OF_defaultNorm[MaxOff+1] = { 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
                                              1, 1, 1, 1, 1, 1, 1, 1,-1,-1,-1,-1,-1 };
static const U32 OF_defaultNormLog = 5;


/*-*******************************************
*  Shared functions to include for inlining
*********************************************/
static void ZSTDv08_copy8(void* dst, const void* src) { memcpy(dst, src, 8); }
#define COPY8(d,s) { ZSTDv08_copy8(d,s); d+=8; s+=8; }

/*! ZSTDv08_wildcopy() :
*   custom version of memcpy(), can copy up to 7 bytes too many (8 bytes if length==0) */
#define WILDCOPY_OVERLENGTH 8
MEM_STATIC void ZSTDv08_wildcopy(void* dst, const void* src, size_t length)
{
    const BYTE* ip = (const BYTE*)src;
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + length;
    do
        COPY8(op, ip)
    while (op < oend);
}


/*-*******************************************
*  Private interfaces
*********************************************/
typedef struct ZSTDv08_stats_s ZSTDv08_stats_t;

typedef struct {
    U32 off;
    U32 len;
} ZSTDv08_match_t;

typedef struct {
    U32 price;
    U32 off;
    U32 mlen;
    U32 litlen;
    U32 rep[ZSTDv08_REP_NUM];
} ZSTDv08_optimal_t;

#if ZSTDv08_OPT_DEBUG == 3
    #include ".debug/zstd_stats.h"
#else
    struct ZSTDv08_stats_s { U32 unused; };
    MEM_STATIC void ZSTDv08_statsPrint(ZSTDv08_stats_t* stats, U32 searchLength) { (void)stats; (void)searchLength; }
    MEM_STATIC void ZSTDv08_statsInit(ZSTDv08_stats_t* stats) { (void)stats; }
    MEM_STATIC void ZSTDv08_statsResetFreqs(ZSTDv08_stats_t* stats) { (void)stats; }
    MEM_STATIC void ZSTDv08_statsUpdatePrices(ZSTDv08_stats_t* stats, size_t litLength, const BYTE* literals, size_t offset, size_t matchLength) { (void)stats; (void)litLength; (void)literals; (void)offset; (void)matchLength; }
#endif   /* #if ZSTDv08_OPT_DEBUG == 3 */


typedef struct seqDef_s {
    U32 offset;
    U16 litLength;
    U16 matchLength;
} seqDef;


typedef struct {
    seqDef* sequencesStart;
    seqDef* sequences;
    BYTE* litStart;
    BYTE* lit;
    BYTE* llCode;
    BYTE* mlCode;
    BYTE* ofCode;
    U32   longLengthID;   /* 0 == no longLength; 1 == Lit.longLength; 2 == Match.longLength; */
    U32   longLengthPos;
    /* opt */
    ZSTDv08_optimal_t* priceTable;
    ZSTDv08_match_t* matchTable;
    U32* matchLengthFreq;
    U32* litLengthFreq;
    U32* litFreq;
    U32* offCodeFreq;
    U32  matchLengthSum;
    U32  matchSum;
    U32  litLengthSum;
    U32  litSum;
    U32  offCodeSum;
    U32  log2matchLengthSum;
    U32  log2matchSum;
    U32  log2litLengthSum;
    U32  log2litSum;
    U32  log2offCodeSum;
    U32  factor;
    U32  cachedPrice;
    U32  cachedLitLength;
    const BYTE* cachedLiterals;
    ZSTDv08_stats_t stats;
} seqStore_t;

void ZSTDv08_seqToCodes(const seqStore_t* seqStorePtr);
int ZSTDv08_isSkipFrame(ZSTDv08_DCtx* dctx);

/* custom memory allocation functions */
void* ZSTDv08_defaultAllocFunction(void* opaque, size_t size);
void ZSTDv08_defaultFreeFunction(void* opaque, void* address);
static const ZSTDv08_customMem defaultCustomMem = { ZSTDv08_defaultAllocFunction, ZSTDv08_defaultFreeFunction, NULL };

/*======  common function  ======*/

MEM_STATIC U32 ZSTDv08_highbit32(U32 val)
{
#   if defined(_MSC_VER)   /* Visual */
    unsigned long r=0;
    _BitScanReverse(&r, val);
    return (unsigned)r;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   /* GCC Intrinsic */
    return 31 - __builtin_clz(val);
#   else   /* Software version */
    static const int DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
    U32 v = val;
    int r;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    r = DeBruijnClz[(U32)(v * 0x07C4ACDDU) >> 27];
    return r;
#   endif
}


#endif   /* ZSTDv08_CCOMMON_H_MODULE */
/*
    zstd - standard compression library
    Copyright (C) 2014-2016, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - zstd homepage : http://www.zstd.net
*/

/* ***************************************************************
*  Tuning parameters
*****************************************************************/
/*!
 * HEAPMODE :
 * Select how default decompression function ZSTDv08_decompress() will allocate memory,
 * in memory stack (0), or in memory heap (1, requires malloc())
 */
#ifndef ZSTDv08_HEAPMODE
#  define ZSTDv08_HEAPMODE 1
#endif



/*-*******************************************************
*  Compiler specifics
*********************************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define FORCE_INLINE static __forceinline
#  include <intrin.h>                    /* For Visual 2005 */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4324)        /* disable: C4324: padded structure */
#else
#  ifdef __GNUC__
#    define FORCE_INLINE static inline __attribute__((always_inline))
#  else
#    define FORCE_INLINE static inline
#  endif
#endif


/*-*************************************
*  Macros
***************************************/
#define ZSTDv08_isError ERR_isError   /* for inlining */
#define FSEv08_isError  ERR_isError
#define HUFv08_isError  ERR_isError


/*_*******************************************************
*  Memory operations
**********************************************************/
static void ZSTDv08_copy4(void* dst, const void* src) { memcpy(dst, src, 4); }


/*-*************************************************************
*   Context management
***************************************************************/
typedef enum { ZSTDds_getFrameHeaderSize, ZSTDds_decodeFrameHeader,
               ZSTDds_decodeBlockHeader, ZSTDds_decompressBlock,
               ZSTDds_decompressLastBlock, ZSTDds_checkChecksum,
               ZSTDds_decodeSkippableHeader, ZSTDds_skipFrame } ZSTDv08_dStage;

struct ZSTDv08_DCtx_s
{
    FSEv08_DTable LLTable[FSEv08_DTABLE_SIZE_U32(LLFSELog)];
    FSEv08_DTable OffTable[FSEv08_DTABLE_SIZE_U32(OffFSELog)];
    FSEv08_DTable MLTable[FSEv08_DTABLE_SIZE_U32(MLFSELog)];
    HUFv08_DTable hufTable[HUFv08_DTABLE_SIZE(HufLog)];  /* can accommodate HUFv08_decompress4X */
    const void* previousDstEnd;
    const void* base;
    const void* vBase;
    const void* dictEnd;
    size_t expected;
    U32 rep[ZSTDv08_REP_NUM];
    ZSTDv08_frameParams fParams;
    blockType_e bType;   /* used in ZSTDv08_decompressContinue(), to transfer blockType between header decoding and block decoding stages */
    ZSTDv08_dStage stage;
    U32 litEntropy;
    U32 fseEntropy;
    XXH64_state_t xxhState;
    size_t headerSize;
    U32 dictID;
    const BYTE* litPtr;
    ZSTDv08_customMem customMem;
    size_t litBufSize;
    size_t litSize;
    size_t rleSize;
    BYTE litBuffer[ZSTDv08_BLOCKSIZE_ABSOLUTEMAX + WILDCOPY_OVERLENGTH];
    BYTE headerBuffer[ZSTDv08_FRAMEHEADERSIZE_MAX];
};  /* typedef'd to ZSTDv08_DCtx within "zstd_static.h" */

size_t ZSTDv08_sizeofDCtx (const ZSTDv08_DCtx* dctx) { return sizeof(*dctx); }

size_t ZSTDv08_estimateDCtxSize(void) { return sizeof(ZSTDv08_DCtx); }

size_t ZSTDv08_decompressBegin(ZSTDv08_DCtx* dctx)
{
    dctx->expected = ZSTDv08_frameHeaderSize_min;
    dctx->stage = ZSTDds_getFrameHeaderSize;
    dctx->previousDstEnd = NULL;
    dctx->base = NULL;
    dctx->vBase = NULL;
    dctx->dictEnd = NULL;
    dctx->hufTable[0] = (HUFv08_DTable)((HufLog)*0x1000001);
    dctx->litEntropy = dctx->fseEntropy = 0;
    dctx->dictID = 0;
    { int i; for (i=0; i<ZSTDv08_REP_NUM; i++) dctx->rep[i] = repStartValue[i]; }
    return 0;
}

ZSTDv08_DCtx* ZSTDv08_createDCtx_advanced(ZSTDv08_customMem customMem)
{
    ZSTDv08_DCtx* dctx;

    if (!customMem.customAlloc && !customMem.customFree)
        customMem = defaultCustomMem;

    if (!customMem.customAlloc || !customMem.customFree)
        return NULL;

    dctx = (ZSTDv08_DCtx*) customMem.customAlloc(customMem.opaque, sizeof(ZSTDv08_DCtx));
    if (!dctx) return NULL;
    memcpy(&dctx->customMem, &customMem, sizeof(ZSTDv08_customMem));
    ZSTDv08_decompressBegin(dctx);
    return dctx;
}

ZSTDv08_DCtx* ZSTDv08_createDCtx(void)
{
    return ZSTDv08_createDCtx_advanced(defaultCustomMem);
}

size_t ZSTDv08_freeDCtx(ZSTDv08_DCtx* dctx)
{
    if (dctx==NULL) return 0;   /* support free on NULL */
    dctx->customMem.customFree(dctx->customMem.opaque, dctx);
    return 0;   /* reserved as a potential error code in the future */
}

void ZSTDv08_copyDCtx(ZSTDv08_DCtx* dstDCtx, const ZSTDv08_DCtx* srcDCtx)
{
    memcpy(dstDCtx, srcDCtx,
           sizeof(ZSTDv08_DCtx) - (ZSTDv08_BLOCKSIZE_ABSOLUTEMAX+WILDCOPY_OVERLENGTH + ZSTDv08_frameHeaderSize_max));  /* no need to copy workspace */
}


/*-*************************************************************
*   Decompression section
***************************************************************/

/* See compression format details in : zstd_compression_format.md */

/** ZSTDv08_frameHeaderSize() :
*   srcSize must be >= ZSTDv08_frameHeaderSize_min.
*   @return : size of the Frame Header */
static size_t ZSTDv08_frameHeaderSize(const void* src, size_t srcSize)
{
    if (srcSize < ZSTDv08_frameHeaderSize_min) return ERROR(srcSize_wrong);
    {   BYTE const fhd = ((const BYTE*)src)[4];
        U32 const dictID= fhd & 3;
        U32 const singleSegment = (fhd >> 5) & 1;
        U32 const fcsId = fhd >> 6;
        return ZSTDv08_frameHeaderSize_min + !singleSegment + ZSTDv08_did_fieldSize[dictID] + ZSTDv08_fcs_fieldSize[fcsId]
                + (singleSegment && !ZSTDv08_fcs_fieldSize[fcsId]);
    }
}


/** ZSTDv08_getFrameParams() :
*   decode Frame Header, or require larger `srcSize`.
*   @return : 0, `fparamsPtr` is correctly filled,
*            >0, `srcSize` is too small, result is expected `srcSize`,
*             or an error code, which can be tested using ZSTDv08_isError() */
size_t ZSTDv08_getFrameParams(ZSTDv08_frameParams* fparamsPtr, const void* src, size_t srcSize)
{
    const BYTE* ip = (const BYTE*)src;

    if (srcSize < ZSTDv08_frameHeaderSize_min) return ZSTDv08_frameHeaderSize_min;
    if (MEM_readLE32(src) != ZSTDv08_MAGICNUMBER) {
        if ((MEM_readLE32(src) & 0xFFFFFFF0U) == ZSTDv08_MAGIC_SKIPPABLE_START) {
            if (srcSize < ZSTDv08_skippableHeaderSize) return ZSTDv08_skippableHeaderSize; /* magic number + skippable frame length */
            memset(fparamsPtr, 0, sizeof(*fparamsPtr));
            fparamsPtr->frameContentSize = MEM_readLE32((const char *)src + 4);
            fparamsPtr->windowSize = 0; /* windowSize==0 means a frame is skippable */
            return 0;
        }
        return ERROR(prefix_unknown);
    }

    /* ensure there is enough `srcSize` to fully read/decode frame header */
    { size_t const fhsize = ZSTDv08_frameHeaderSize(src, srcSize);
      if (srcSize < fhsize) return fhsize; }

    {   BYTE const fhdByte = ip[4];
        size_t pos = 5;
        U32 const dictIDSizeCode = fhdByte&3;
        U32 const checksumFlag = (fhdByte>>2)&1;
        U32 const singleSegment = (fhdByte>>5)&1;
        U32 const fcsID = fhdByte>>6;
        U32 const windowSizeMax = 1U << ZSTDv08_WINDOWLOG_MAX;
        U32 windowSize = 0;
        U32 dictID = 0;
        U64 frameContentSize = 0;
        if ((fhdByte & 0x08) != 0) return ERROR(frameParameter_unsupported);   /* reserved bits, which must be zero */
        if (!singleSegment) {
            BYTE const wlByte = ip[pos++];
            U32 const windowLog = (wlByte >> 3) + ZSTDv08_WINDOWLOG_ABSOLUTEMIN;
            if (windowLog > ZSTDv08_WINDOWLOG_MAX) return ERROR(frameParameter_unsupported);
            windowSize = (1U << windowLog);
            windowSize += (windowSize >> 3) * (wlByte&7);
        }

        switch(dictIDSizeCode)
        {
            default:   /* impossible */
            case 0 : break;
            case 1 : dictID = ip[pos]; pos++; break;
            case 2 : dictID = MEM_readLE16(ip+pos); pos+=2; break;
            case 3 : dictID = MEM_readLE32(ip+pos); pos+=4; break;
        }
        switch(fcsID)
        {
            default:   /* impossible */
            case 0 : if (singleSegment) frameContentSize = ip[pos]; break;
            case 1 : frameContentSize = MEM_readLE16(ip+pos)+256; break;
            case 2 : frameContentSize = MEM_readLE32(ip+pos); break;
            case 3 : frameContentSize = MEM_readLE64(ip+pos); break;
        }
        if (!windowSize) windowSize = (U32)frameContentSize;
        if (windowSize > windowSizeMax) return ERROR(frameParameter_unsupported);
        fparamsPtr->frameContentSize = frameContentSize;
        fparamsPtr->windowSize = windowSize;
        fparamsPtr->dictID = dictID;
        fparamsPtr->checksumFlag = checksumFlag;
    }
    return 0;
}


/** ZSTDv08_getDecompressedSize() :
*   compatible with legacy mode
*   @return : decompressed size if known, 0 otherwise
              note : 0 can mean any of the following :
                   - decompressed size is not present within frame header
                   - frame header unknown / not supported
                   - frame header not complete (`srcSize` too small) */
unsigned long long ZSTDv08_getDecompressedSize(const void* src, size_t srcSize)
{
    {   ZSTDv08_frameParams fparams;
        size_t const frResult = ZSTDv08_getFrameParams(&fparams, src, srcSize);
        if (frResult!=0) return 0;
        return fparams.frameContentSize;
    }
}


/** ZSTDv08_decodeFrameHeader() :
*   `srcSize` must be the size provided by ZSTDv08_frameHeaderSize().
*   @return : 0 if success, or an error code, which can be tested using ZSTDv08_isError() */
static size_t ZSTDv08_decodeFrameHeader(ZSTDv08_DCtx* dctx, const void* src, size_t srcSize)
{
    size_t const result = ZSTDv08_getFrameParams(&(dctx->fParams), src, srcSize);
    if (dctx->fParams.dictID && (dctx->dictID != dctx->fParams.dictID)) return ERROR(dictionary_wrong);
    if (dctx->fParams.checksumFlag) XXH64_reset(&dctx->xxhState, 0);
    return result;
}


typedef struct
{
    blockType_e blockType;
    U32 lastBlock;
    U32 origSize;
} blockProperties_t;

/*! ZSTDv08_getcBlockSize() :
*   Provides the size of compressed block from block header `src` */
size_t ZSTDv08_getcBlockSize(const void* src, size_t srcSize, blockProperties_t* bpPtr)
{
    if (srcSize < ZSTDv08_blockHeaderSize) return ERROR(srcSize_wrong);
    {   U32 const cBlockHeader = MEM_readLE24(src);
        U32 const cSize = cBlockHeader >> 3;
        bpPtr->lastBlock = cBlockHeader & 1;
        bpPtr->blockType = (blockType_e)((cBlockHeader >> 1) & 3);
        bpPtr->origSize = cSize;   /* only useful for RLE */
        if (bpPtr->blockType == bt_rle) return 1;
        if (bpPtr->blockType == bt_reserved) return ERROR(corruption_detected);
        return cSize;
    }
}


static size_t ZSTDv08_copyRawBlock(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    if (srcSize > dstCapacity) return ERROR(dstSize_tooSmall);
    memcpy(dst, src, srcSize);
    return srcSize;
}


static size_t ZSTDv08_setRleBlock(void* dst, size_t dstCapacity, const void* src, size_t srcSize, size_t regenSize)
{
    if (srcSize != 1) return ERROR(srcSize_wrong);
    if (regenSize > dstCapacity) return ERROR(dstSize_tooSmall);
    memset(dst, *(const BYTE*)src, regenSize);
    return regenSize;
}

/*! ZSTDv08_decodeLiteralsBlock() :
    @return : nb of bytes read from src (< srcSize ) */
size_t ZSTDv08_decodeLiteralsBlock(ZSTDv08_DCtx* dctx,
                          const void* src, size_t srcSize)   /* note : srcSize < BLOCKSIZE */
{
    if (srcSize < MIN_CBLOCK_SIZE) return ERROR(corruption_detected);

    {   const BYTE* const istart = (const BYTE*) src;
        symbolEncodingType_e const litEncType = (symbolEncodingType_e)(istart[0] & 3);

        switch(litEncType)
        {
        case set_repeat:
            if (dctx->litEntropy==0) return ERROR(dictionary_corrupted);
            /* fall-through */
        case set_compressed:
            if (srcSize < 5) return ERROR(corruption_detected);   /* srcSize >= MIN_CBLOCK_SIZE == 3; here we need up to 5 for case 3 */
            {   size_t lhSize, litSize, litCSize;
                U32 singleStream=0;
                U32 const lhlCode = (istart[0] >> 2) & 3;
                U32 const lhc = MEM_readLE32(istart);
                switch(lhlCode)
                {
                case 0: case 1: default:   /* note : default is impossible, since lhlCode into [0..3] */
                    /* 2 - 2 - 10 - 10 */
                    {   singleStream = !lhlCode;
                        lhSize = 3;
                        litSize  = (lhc >> 4) & 0x3FF;
                        litCSize = (lhc >> 14) & 0x3FF;
                        break;
                    }
                case 2:
                    /* 2 - 2 - 14 - 14 */
                    {   lhSize = 4;
                        litSize  = (lhc >> 4) & 0x3FFF;
                        litCSize = lhc >> 18;
                        break;
                    }
                case 3:
                    /* 2 - 2 - 18 - 18 */
                    {   lhSize = 5;
                        litSize  = (lhc >> 4) & 0x3FFFF;
                        litCSize = (lhc >> 22) + (istart[4] << 10);
                        break;
                    }
                }
                if (litSize > ZSTDv08_BLOCKSIZE_ABSOLUTEMAX) return ERROR(corruption_detected);
                if (litCSize + lhSize > srcSize) return ERROR(corruption_detected);

                if (HUFv08_isError((litEncType==set_repeat) ?
                                    ( singleStream ?
                                        HUFv08_decompress1X_usingDTable(dctx->litBuffer, litSize, istart+lhSize, litCSize, dctx->hufTable) :
                                        HUFv08_decompress4X_usingDTable(dctx->litBuffer, litSize, istart+lhSize, litCSize, dctx->hufTable) ) :
                                    ( singleStream ?
                                        HUFv08_decompress1X2_DCtx(dctx->hufTable, dctx->litBuffer, litSize, istart+lhSize, litCSize) :
                                        HUFv08_decompress4X_hufOnly (dctx->hufTable, dctx->litBuffer, litSize, istart+lhSize, litCSize)) ))
                    return ERROR(corruption_detected);

                dctx->litPtr = dctx->litBuffer;
                dctx->litBufSize = ZSTDv08_BLOCKSIZE_ABSOLUTEMAX+WILDCOPY_OVERLENGTH;
                dctx->litSize = litSize;
                dctx->litEntropy = 1;
                return litCSize + lhSize;
            }

        case set_basic:
            {   size_t litSize, lhSize;
                U32 const lhlCode = ((istart[0]) >> 2) & 3;
                switch(lhlCode)
                {
                case 0: case 2: default:   /* note : default is impossible, since lhlCode into [0..3] */
                    lhSize = 1;
                    litSize = istart[0] >> 3;
                    break;
                case 1:
                    lhSize = 2;
                    litSize = MEM_readLE16(istart) >> 4;
                    break;
                case 3:
                    lhSize = 3;
                    litSize = MEM_readLE24(istart) >> 4;
                    break;
                }

                if (lhSize+litSize+WILDCOPY_OVERLENGTH > srcSize) {  /* risk reading beyond src buffer with wildcopy */
                    if (litSize+lhSize > srcSize) return ERROR(corruption_detected);
                    memcpy(dctx->litBuffer, istart+lhSize, litSize);
                    dctx->litPtr = dctx->litBuffer;
                    dctx->litBufSize = ZSTDv08_BLOCKSIZE_ABSOLUTEMAX+8;
                    dctx->litSize = litSize;
                    return lhSize+litSize;
                }
                /* direct reference into compressed stream */
                dctx->litPtr = istart+lhSize;
                dctx->litBufSize = srcSize-lhSize;
                dctx->litSize = litSize;
                return lhSize+litSize;
            }

        case set_rle:
            {   U32 const lhlCode = ((istart[0]) >> 2) & 3;
                size_t litSize, lhSize;
                switch(lhlCode)
                {
                case 0: case 2: default:   /* note : default is impossible, since lhlCode into [0..3] */
                    lhSize = 1;
                    litSize = istart[0] >> 3;
                    break;
                case 1:
                    lhSize = 2;
                    litSize = MEM_readLE16(istart) >> 4;
                    break;
                case 3:
                    lhSize = 3;
                    litSize = MEM_readLE24(istart) >> 4;
                    if (srcSize<4) return ERROR(corruption_detected);   /* srcSize >= MIN_CBLOCK_SIZE == 3; here we need lhSize+1 = 4 */
                    break;
                }
                if (litSize > ZSTDv08_BLOCKSIZE_ABSOLUTEMAX) return ERROR(corruption_detected);
                memset(dctx->litBuffer, istart[lhSize], litSize);
                dctx->litPtr = dctx->litBuffer;
                dctx->litBufSize = ZSTDv08_BLOCKSIZE_ABSOLUTEMAX+WILDCOPY_OVERLENGTH;
                dctx->litSize = litSize;
                return lhSize+1;
            }
        default:
            return ERROR(corruption_detected);   /* impossible */
        }

    }
}


/*! ZSTDv08_buildSeqTable() :
    @return : nb bytes read from src,
              or an error code if it fails, testable with ZSTDv08_isError()
*/
FORCE_INLINE size_t ZSTDv08_buildSeqTable(FSEv08_DTable* DTable, symbolEncodingType_e type, U32 max, U32 maxLog,
                                 const void* src, size_t srcSize,
                                 const S16* defaultNorm, U32 defaultLog, U32 flagRepeatTable)
{
    switch(type)
    {
    case set_rle :
        if (!srcSize) return ERROR(srcSize_wrong);
        if ( (*(const BYTE*)src) > max) return ERROR(corruption_detected);
        FSEv08_buildDTable_rle(DTable, *(const BYTE*)src);   /* if *src > max, data is corrupted */
        return 1;
    case set_basic :
        FSEv08_buildDTable(DTable, defaultNorm, max, defaultLog);
        return 0;
    case set_repeat:
        if (!flagRepeatTable) return ERROR(corruption_detected);
        return 0;
    default :   /* impossible */
    case set_compressed :
        {   U32 tableLog;
            S16 norm[MaxSeq+1];
            size_t const headerSize = FSEv08_readNCount(norm, &max, &tableLog, src, srcSize);
            if (FSEv08_isError(headerSize)) return ERROR(corruption_detected);
            if (tableLog > maxLog) return ERROR(corruption_detected);
            FSEv08_buildDTable(DTable, norm, max, tableLog);
            return headerSize;
    }   }
}


size_t ZSTDv08_decodeSeqHeaders(int* nbSeqPtr,
                             FSEv08_DTable* DTableLL, FSEv08_DTable* DTableML, FSEv08_DTable* DTableOffb, U32 flagRepeatTable,
                             const void* src, size_t srcSize)
{
    const BYTE* const istart = (const BYTE* const)src;
    const BYTE* const iend = istart + srcSize;
    const BYTE* ip = istart;

    /* check */
    if (srcSize < MIN_SEQUENCES_SIZE) return ERROR(srcSize_wrong);

    /* SeqHead */
    {   int nbSeq = *ip++;
        if (!nbSeq) { *nbSeqPtr=0; return 1; }
        if (nbSeq > 0x7F) {
            if (nbSeq == 0xFF)
                nbSeq = MEM_readLE16(ip) + LONGNBSEQ, ip+=2;
            else
                nbSeq = ((nbSeq-0x80)<<8) + *ip++;
        }
        *nbSeqPtr = nbSeq;
    }

    /* FSE table descriptors */
    if (ip+4 > iend) return ERROR(srcSize_wrong); /* minimum possible size */
    {   symbolEncodingType_e const LLtype = (symbolEncodingType_e)(*ip >> 6);
        symbolEncodingType_e const OFtype = (symbolEncodingType_e)((*ip >> 4) & 3);
        symbolEncodingType_e const MLtype = (symbolEncodingType_e)((*ip >> 2) & 3);
        ip++;

        /* Build DTables */
        {   size_t const llhSize = ZSTDv08_buildSeqTable(DTableLL, LLtype, MaxLL, LLFSELog, ip, iend-ip, LL_defaultNorm, LL_defaultNormLog, flagRepeatTable);
            if (ZSTDv08_isError(llhSize)) return ERROR(corruption_detected);
            ip += llhSize;
        }
        {   size_t const ofhSize = ZSTDv08_buildSeqTable(DTableOffb, OFtype, MaxOff, OffFSELog, ip, iend-ip, OF_defaultNorm, OF_defaultNormLog, flagRepeatTable);
            if (ZSTDv08_isError(ofhSize)) return ERROR(corruption_detected);
            ip += ofhSize;
        }
        {   size_t const mlhSize = ZSTDv08_buildSeqTable(DTableML, MLtype, MaxML, MLFSELog, ip, iend-ip, ML_defaultNorm, ML_defaultNormLog, flagRepeatTable);
            if (ZSTDv08_isError(mlhSize)) return ERROR(corruption_detected);
            ip += mlhSize;
    }   }

    return ip-istart;
}


typedef struct {
    size_t litLength;
    size_t matchLength;
    size_t offset;
} seq_t;

typedef struct {
    BIT_DStream_t DStream;
    FSEv08_DState_t stateLL;
    FSEv08_DState_t stateOffb;
    FSEv08_DState_t stateML;
    size_t prevOffset[ZSTDv08_REP_NUM];
} seqState_t;


static seq_t ZSTDv08_decodeSequence(seqState_t* seqState)
{
    seq_t seq;

    U32 const llCode = FSEv08_peekSymbol(&(seqState->stateLL));
    U32 const mlCode = FSEv08_peekSymbol(&(seqState->stateML));
    U32 const ofCode = FSEv08_peekSymbol(&(seqState->stateOffb));   /* <= maxOff, by table construction */

    U32 const llBits = LL_bits[llCode];
    U32 const mlBits = ML_bits[mlCode];
    U32 const ofBits = ofCode;
    U32 const totalBits = llBits+mlBits+ofBits;

    static const U32 LL_base[MaxLL+1] = {
                             0,  1,  2,  3,  4,  5,  6,  7,  8,  9,   10,    11,    12,    13,    14,     15,
                            16, 18, 20, 22, 24, 28, 32, 40, 48, 64, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000,
                            0x2000, 0x4000, 0x8000, 0x10000 };

    static const U32 ML_base[MaxML+1] = {
                             3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,   14,    15,    16,    17,    18,
                            19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,   30,    31,    32,    33,    34,
                            35, 37, 39, 41, 43, 47, 51, 59, 67, 83, 99, 0x83, 0x103, 0x203, 0x403, 0x803,
                            0x1003, 0x2003, 0x4003, 0x8003, 0x10003 };

    static const U32 OF_base[MaxOff+1] = {
                 0,        1,       1,       5,     0xD,     0x1D,     0x3D,     0x7D,
                 0xFD,   0x1FD,   0x3FD,   0x7FD,   0xFFD,   0x1FFD,   0x3FFD,   0x7FFD,
                 0xFFFD, 0x1FFFD, 0x3FFFD, 0x7FFFD, 0xFFFFD, 0x1FFFFD, 0x3FFFFD, 0x7FFFFD,
                 0xFFFFFD, 0x1FFFFFD, 0x3FFFFFD, 0x7FFFFFD, 0xFFFFFFD };

    /* sequence */
    {   size_t offset;
        if (!ofCode)
            offset = 0;
        else {
            offset = OF_base[ofCode] + BIT_readBits(&(seqState->DStream), ofBits);   /* <=  (ZSTDv08_WINDOWLOG_MAX-1) bits */
            if (MEM_32bits()) BIT_reloadDStream(&(seqState->DStream));
        }

        if (ofCode <= 1) {
            offset += (llCode==0);
            if (offset) {
                size_t const temp = (offset==3) ? seqState->prevOffset[0] - 1 : seqState->prevOffset[offset];
                if (offset != 1) seqState->prevOffset[2] = seqState->prevOffset[1];
                seqState->prevOffset[1] = seqState->prevOffset[0];
                seqState->prevOffset[0] = offset = temp;
            } else {
                offset = seqState->prevOffset[0];
            }
        } else {
            seqState->prevOffset[2] = seqState->prevOffset[1];
            seqState->prevOffset[1] = seqState->prevOffset[0];
            seqState->prevOffset[0] = offset;
        }
        seq.offset = offset;
    }

    seq.matchLength = ML_base[mlCode] + ((mlCode>31) ? BIT_readBits(&(seqState->DStream), mlBits) : 0);   /* <=  16 bits */
    if (MEM_32bits() && (mlBits+llBits>24)) BIT_reloadDStream(&(seqState->DStream));

    seq.litLength = LL_base[llCode] + ((llCode>15) ? BIT_readBits(&(seqState->DStream), llBits) : 0);   /* <=  16 bits */
    if (MEM_32bits() ||
       (totalBits > 64 - 7 - (LLFSELog+MLFSELog+OffFSELog)) ) BIT_reloadDStream(&(seqState->DStream));

    /* ANS state update */
    FSEv08_updateState(&(seqState->stateLL), &(seqState->DStream));   /* <=  9 bits */
    FSEv08_updateState(&(seqState->stateML), &(seqState->DStream));   /* <=  9 bits */
    if (MEM_32bits()) BIT_reloadDStream(&(seqState->DStream));     /* <= 18 bits */
    FSEv08_updateState(&(seqState->stateOffb), &(seqState->DStream)); /* <=  8 bits */

    return seq;
}


FORCE_INLINE
size_t ZSTDv08_execSequence(BYTE* op,
                                BYTE* const oend, seq_t sequence,
                                const BYTE** litPtr, const BYTE* const litLimit_w,
                                const BYTE* const base, const BYTE* const vBase, const BYTE* const dictEnd)
{
    BYTE* const oLitEnd = op + sequence.litLength;
    size_t const sequenceLength = sequence.litLength + sequence.matchLength;
    BYTE* const oMatchEnd = op + sequenceLength;   /* risk : address space overflow (32-bits) */
    BYTE* const oend_w = oend - WILDCOPY_OVERLENGTH;
    const BYTE* const iLitEnd = *litPtr + sequence.litLength;
    const BYTE* match = oLitEnd - sequence.offset;

    /* check */
    if ((oLitEnd>oend_w) | (oMatchEnd>oend)) return ERROR(dstSize_tooSmall); /* last match must start at a minimum distance of WILDCOPY_OVERLENGTH from oend */
    if (iLitEnd > litLimit_w) return ERROR(corruption_detected);   /* over-read beyond lit buffer */

    /* copy Literals */
    ZSTDv08_wildcopy(op, *litPtr, sequence.litLength);   /* note : since oLitEnd <= oend-WILDCOPY_OVERLENGTH, no risk of overwrite beyond oend */
    op = oLitEnd;
    *litPtr = iLitEnd;   /* update for next sequence */

    /* copy Match */
    if (sequence.offset > (size_t)(oLitEnd - base)) {
        /* offset beyond prefix */
        if (sequence.offset > (size_t)(oLitEnd - vBase)) return ERROR(corruption_detected);
        match = dictEnd - (base-match);
        if (match + sequence.matchLength <= dictEnd) {
            memmove(oLitEnd, match, sequence.matchLength);
            return sequenceLength;
        }
        /* span extDict & currentPrefixSegment */
        {   size_t const length1 = dictEnd - match;
            memmove(oLitEnd, match, length1);
            op = oLitEnd + length1;
            sequence.matchLength -= length1;
            match = base;
    }   }

    /* match within prefix */
    if (sequence.offset < 8) {
        /* close range match, overlap */
        static const U32 dec32table[] = { 0, 1, 2, 1, 4, 4, 4, 4 };   /* added */
        static const int dec64table[] = { 8, 8, 8, 7, 8, 9,10,11 };   /* substracted */
        int const sub2 = dec64table[sequence.offset];
        op[0] = match[0];
        op[1] = match[1];
        op[2] = match[2];
        op[3] = match[3];
        match += dec32table[sequence.offset];
        ZSTDv08_copy4(op+4, match);
        match -= sub2;
    } else {
        ZSTDv08_copy8(op, match);
    }
    op += 8; match += 8;

    if (oMatchEnd > oend-(16-MINMATCH)) {
        if (op < oend_w) {
            ZSTDv08_wildcopy(op, match, oend_w - op);
            match += oend_w - op;
            op = oend_w;
        }
        while (op < oMatchEnd) *op++ = *match++;
    } else {
        ZSTDv08_wildcopy(op, match, sequence.matchLength-8);   /* works even if matchLength < 8 */
    }
    return sequenceLength;
}


static size_t ZSTDv08_decompressSequences(
                               ZSTDv08_DCtx* dctx,
                               void* dst, size_t maxDstSize,
                         const void* seqStart, size_t seqSize)
{
    const BYTE* ip = (const BYTE*)seqStart;
    const BYTE* const iend = ip + seqSize;
    BYTE* const ostart = (BYTE* const)dst;
    BYTE* const oend = ostart + maxDstSize;
    BYTE* op = ostart;
    const BYTE* litPtr = dctx->litPtr;
    const BYTE* const litLimit_w = litPtr + dctx->litBufSize - WILDCOPY_OVERLENGTH;
    const BYTE* const litEnd = litPtr + dctx->litSize;
    FSEv08_DTable* DTableLL = dctx->LLTable;
    FSEv08_DTable* DTableML = dctx->MLTable;
    FSEv08_DTable* DTableOffb = dctx->OffTable;
    const BYTE* const base = (const BYTE*) (dctx->base);
    const BYTE* const vBase = (const BYTE*) (dctx->vBase);
    const BYTE* const dictEnd = (const BYTE*) (dctx->dictEnd);
    int nbSeq;

    /* Build Decoding Tables */
    {   size_t const seqHSize = ZSTDv08_decodeSeqHeaders(&nbSeq, DTableLL, DTableML, DTableOffb, dctx->fseEntropy, ip, seqSize);
        if (ZSTDv08_isError(seqHSize)) return seqHSize;
        ip += seqHSize;
    }

    /* Regen sequences */
    if (nbSeq) {
        seqState_t seqState;
        dctx->fseEntropy = 1;
        { U32 i; for (i=0; i<ZSTDv08_REP_NUM; i++) seqState.prevOffset[i] = dctx->rep[i]; }
        { size_t const errorCode = BIT_initDStream(&(seqState.DStream), ip, iend-ip);
          if (ERR_isError(errorCode)) return ERROR(corruption_detected); }
        FSEv08_initDState(&(seqState.stateLL), &(seqState.DStream), DTableLL);
        FSEv08_initDState(&(seqState.stateOffb), &(seqState.DStream), DTableOffb);
        FSEv08_initDState(&(seqState.stateML), &(seqState.DStream), DTableML);

        for ( ; (BIT_reloadDStream(&(seqState.DStream)) <= BIT_DStream_completed) && nbSeq ; ) {
            nbSeq--;
            {   seq_t const sequence = ZSTDv08_decodeSequence(&seqState);
                size_t const oneSeqSize = ZSTDv08_execSequence(op, oend, sequence, &litPtr, litLimit_w, base, vBase, dictEnd);
                if (ZSTDv08_isError(oneSeqSize)) return oneSeqSize;
                op += oneSeqSize;
        }   }

        /* check if reached exact end */
        if (nbSeq) return ERROR(corruption_detected);
        /* save reps for next block */
        { U32 i; for (i=0; i<ZSTDv08_REP_NUM; i++) dctx->rep[i] = (U32)(seqState.prevOffset[i]); }
    }

    /* last literal segment */
    {   size_t const lastLLSize = litEnd - litPtr;
        if (lastLLSize > (size_t)(oend-op)) return ERROR(dstSize_tooSmall);
        memcpy(op, litPtr, lastLLSize);
        op += lastLLSize;
    }

    return op-ostart;
}


static void ZSTDv08_checkContinuity(ZSTDv08_DCtx* dctx, const void* dst)
{
    if (dst != dctx->previousDstEnd) {   /* not contiguous */
        dctx->dictEnd = dctx->previousDstEnd;
        dctx->vBase = (const char*)dst - ((const char*)(dctx->previousDstEnd) - (const char*)(dctx->base));
        dctx->base = dst;
        dctx->previousDstEnd = dst;
    }
}


static size_t ZSTDv08_decompressBlock_internal(ZSTDv08_DCtx* dctx,
                            void* dst, size_t dstCapacity,
                      const void* src, size_t srcSize)
{   /* blockType == blockCompressed */
    const BYTE* ip = (const BYTE*)src;

    if (srcSize >= ZSTDv08_BLOCKSIZE_ABSOLUTEMAX) return ERROR(srcSize_wrong);

    /* Decode literals sub-block */
    {   size_t const litCSize = ZSTDv08_decodeLiteralsBlock(dctx, src, srcSize);
        if (ZSTDv08_isError(litCSize)) return litCSize;
        ip += litCSize;
        srcSize -= litCSize;
    }
    return ZSTDv08_decompressSequences(dctx, dst, dstCapacity, ip, srcSize);
}


size_t ZSTDv08_decompressBlock(ZSTDv08_DCtx* dctx,
                            void* dst, size_t dstCapacity,
                      const void* src, size_t srcSize)
{
    size_t dSize;
    ZSTDv08_checkContinuity(dctx, dst);
    dSize = ZSTDv08_decompressBlock_internal(dctx, dst, dstCapacity, src, srcSize);
    dctx->previousDstEnd = (char*)dst + dSize;
    return dSize;
}


/** ZSTDv08_insertBlock() :
    insert `src` block into `dctx` history. Useful to track uncompressed blocks. */
ZSTDLIB_API size_t ZSTDv08_insertBlock(ZSTDv08_DCtx* dctx, const void* blockStart, size_t blockSize)
{
    ZSTDv08_checkContinuity(dctx, blockStart);
    dctx->previousDstEnd = (const char*)blockStart + blockSize;
    return blockSize;
}


size_t ZSTDv08_generateNxBytes(void* dst, size_t dstCapacity, BYTE byte, size_t length)
{
    if (length > dstCapacity) return ERROR(dstSize_tooSmall);
    memset(dst, byte, length);
    return length;
}


/*! ZSTDv08_decompressFrame() :
*   `dctx` must be properly initialized */
static size_t ZSTDv08_decompressFrame(ZSTDv08_DCtx* dctx,
                                 void* dst, size_t dstCapacity,
                                 const void* src, size_t srcSize)
{
    const BYTE* ip = (const BYTE*)src;
    BYTE* const ostart = (BYTE* const)dst;
    BYTE* const oend = ostart + dstCapacity;
    BYTE* op = ostart;
    size_t remainingSize = srcSize;

    /* check */
    if (srcSize < ZSTDv08_frameHeaderSize_min+ZSTDv08_blockHeaderSize) return ERROR(srcSize_wrong);

    /* Frame Header */
    {   size_t const frameHeaderSize = ZSTDv08_frameHeaderSize(src, ZSTDv08_frameHeaderSize_min);
        size_t result;
        if (ZSTDv08_isError(frameHeaderSize)) return frameHeaderSize;
        if (srcSize < frameHeaderSize+ZSTDv08_blockHeaderSize) return ERROR(srcSize_wrong);
        result = ZSTDv08_decodeFrameHeader(dctx, src, frameHeaderSize);
        if (ZSTDv08_isError(result)) return result;
        ip += frameHeaderSize; remainingSize -= frameHeaderSize;
    }

    /* Loop on each block */
    while (1) {
        size_t decodedSize;
        blockProperties_t blockProperties;
        size_t const cBlockSize = ZSTDv08_getcBlockSize(ip, remainingSize, &blockProperties);
        if (ZSTDv08_isError(cBlockSize)) return cBlockSize;

        ip += ZSTDv08_blockHeaderSize;
        remainingSize -= ZSTDv08_blockHeaderSize;
        if (cBlockSize > remainingSize) return ERROR(srcSize_wrong);

        switch(blockProperties.blockType)
        {
        case bt_compressed:
            decodedSize = ZSTDv08_decompressBlock_internal(dctx, op, oend-op, ip, cBlockSize);
            break;
        case bt_raw :
            decodedSize = ZSTDv08_copyRawBlock(op, oend-op, ip, cBlockSize);
            break;
        case bt_rle :
            decodedSize = ZSTDv08_generateNxBytes(op, oend-op, *ip, blockProperties.origSize);
            break;
        case bt_reserved :
        default:
            return ERROR(corruption_detected);
        }

        if (ZSTDv08_isError(decodedSize)) return decodedSize;
        if (dctx->fParams.checksumFlag) XXH64_update(&dctx->xxhState, op, decodedSize);
        op += decodedSize;
        ip += cBlockSize;
        remainingSize -= cBlockSize;
        if (blockProperties.lastBlock) break;
    }

    if (dctx->fParams.checksumFlag) {   /* Frame content checksum verification */
        U32 const checkCalc = (U32)XXH64_digest(&dctx->xxhState);
        U32 checkRead;
        if (remainingSize<4) return ERROR(checksum_wrong);
        checkRead = MEM_readLE32(ip);
        if (checkRead != checkCalc) return ERROR(checksum_wrong);
        remainingSize -= 4;
    }

    if (remainingSize) return ERROR(srcSize_wrong);
    return op-ostart;
}


/*! ZSTDv08_decompress_usingPreparedDCtx() :
*   Same as ZSTDv08_decompress_usingDict, but using a reference context `preparedDCtx`, where dictionary has been loaded.
*   It avoids reloading the dictionary each time.
*   `preparedDCtx` must have been properly initialized using ZSTDv08_decompressBegin_usingDict().
*   Requires 2 contexts : 1 for reference (preparedDCtx), which will not be modified, and 1 to run the decompression operation (dctx) */
size_t ZSTDv08_decompress_usingPreparedDCtx(ZSTDv08_DCtx* dctx, const ZSTDv08_DCtx* refDCtx,
                                         void* dst, size_t dstCapacity,
                                   const void* src, size_t srcSize)
{
    ZSTDv08_copyDCtx(dctx, refDCtx);
    ZSTDv08_checkContinuity(dctx, dst);
    return ZSTDv08_decompressFrame(dctx, dst, dstCapacity, src, srcSize);
}


size_t ZSTDv08_decompress_usingDict(ZSTDv08_DCtx* dctx,
                                 void* dst, size_t dstCapacity,
                                 const void* src, size_t srcSize,
                                 const void* dict, size_t dictSize)
{
    ZSTDv08_decompressBegin_usingDict(dctx, dict, dictSize);
    ZSTDv08_checkContinuity(dctx, dst);
    return ZSTDv08_decompressFrame(dctx, dst, dstCapacity, src, srcSize);
}


size_t ZSTDv08_decompressDCtx(ZSTDv08_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    return ZSTDv08_decompress_usingDict(dctx, dst, dstCapacity, src, srcSize, NULL, 0);
}


size_t ZSTDv08_decompress(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
#if defined(ZSTDv08_HEAPMODE) && (ZSTDv08_HEAPMODE==1)
    size_t regenSize;
    ZSTDv08_DCtx* const dctx = ZSTDv08_createDCtx();
    if (dctx==NULL) return ERROR(memory_allocation);
    regenSize = ZSTDv08_decompressDCtx(dctx, dst, dstCapacity, src, srcSize);
    ZSTDv08_freeDCtx(dctx);
    return regenSize;
#else   /* stack mode */
    ZSTDv08_DCtx dctx;
    return ZSTDv08_decompressDCtx(&dctx, dst, dstCapacity, src, srcSize);
#endif
}


/*-**********************************
*   Streaming Decompression API
************************************/
size_t ZSTDv08_nextSrcSizeToDecompress(ZSTDv08_DCtx* dctx) { return dctx->expected; }

ZSTDv08_nextInputType_e ZSTDv08_nextInputType(ZSTDv08_DCtx* dctx) {
    switch(dctx->stage)
    {
    default:   /* should not happen */
    case ZSTDds_getFrameHeaderSize:
    case ZSTDds_decodeFrameHeader:
        return ZSTDnit_frameHeader;
    case ZSTDds_decodeBlockHeader:
        return ZSTDnit_blockHeader;
    case ZSTDds_decompressBlock:
        return ZSTDnit_block;
    case ZSTDds_decompressLastBlock:
        return ZSTDnit_lastBlock;
    case ZSTDds_checkChecksum:
        return ZSTDnit_checksum;
    case ZSTDds_decodeSkippableHeader:
    case ZSTDds_skipFrame:
        return ZSTDnit_skippableFrame;
    }
}

int ZSTDv08_isSkipFrame(ZSTDv08_DCtx* dctx) { return dctx->stage == ZSTDds_skipFrame; }   /* for zbuff */

/** ZSTDv08_decompressContinue() :
*   @return : nb of bytes generated into `dst` (necessarily <= `dstCapacity)
*             or an error code, which can be tested using ZSTDv08_isError() */
size_t ZSTDv08_decompressContinue(ZSTDv08_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    /* Sanity check */
    if (srcSize != dctx->expected) return ERROR(srcSize_wrong);
    if (dstCapacity) ZSTDv08_checkContinuity(dctx, dst);

    switch (dctx->stage)
    {
    case ZSTDds_getFrameHeaderSize :
        if (srcSize != ZSTDv08_frameHeaderSize_min) return ERROR(srcSize_wrong);   /* impossible */
        if ((MEM_readLE32(src) & 0xFFFFFFF0U) == ZSTDv08_MAGIC_SKIPPABLE_START) {
            memcpy(dctx->headerBuffer, src, ZSTDv08_frameHeaderSize_min);
            dctx->expected = ZSTDv08_skippableHeaderSize - ZSTDv08_frameHeaderSize_min; /* magic number + skippable frame length */
            dctx->stage = ZSTDds_decodeSkippableHeader;
            return 0;
        }
        dctx->headerSize = ZSTDv08_frameHeaderSize(src, ZSTDv08_frameHeaderSize_min);
        if (ZSTDv08_isError(dctx->headerSize)) return dctx->headerSize;
        memcpy(dctx->headerBuffer, src, ZSTDv08_frameHeaderSize_min);
        if (dctx->headerSize > ZSTDv08_frameHeaderSize_min) {
            dctx->expected = dctx->headerSize - ZSTDv08_frameHeaderSize_min;
            dctx->stage = ZSTDds_decodeFrameHeader;
            return 0;
        }
        dctx->expected = 0;   /* not necessary to copy more */

    case ZSTDds_decodeFrameHeader:
        {   size_t result;
            memcpy(dctx->headerBuffer + ZSTDv08_frameHeaderSize_min, src, dctx->expected);
            result = ZSTDv08_decodeFrameHeader(dctx, dctx->headerBuffer, dctx->headerSize);
            if (ZSTDv08_isError(result)) return result;
            dctx->expected = ZSTDv08_blockHeaderSize;
            dctx->stage = ZSTDds_decodeBlockHeader;
            return 0;
        }
    case ZSTDds_decodeBlockHeader:
        {   blockProperties_t bp;
            size_t const cBlockSize = ZSTDv08_getcBlockSize(src, ZSTDv08_blockHeaderSize, &bp);
            if (ZSTDv08_isError(cBlockSize)) return cBlockSize;
            dctx->expected = cBlockSize;
            dctx->bType = bp.blockType;
            dctx->rleSize = bp.origSize;
            if (cBlockSize) {
                dctx->stage = bp.lastBlock ? ZSTDds_decompressLastBlock : ZSTDds_decompressBlock;
                return 0;
            }
            /* empty block */
            if (bp.lastBlock) {
                if (dctx->fParams.checksumFlag) {
                    dctx->expected = 4;
                    dctx->stage = ZSTDds_checkChecksum;
                } else {
                    dctx->expected = 0; /* end of frame */
                    dctx->stage = ZSTDds_getFrameHeaderSize;
                }
            } else {
                dctx->expected = 3;  /* go directly to next header */
                dctx->stage = ZSTDds_decodeBlockHeader;
            }
            return 0;
        }
    case ZSTDds_decompressLastBlock:
    case ZSTDds_decompressBlock:
        {   size_t rSize;
            switch(dctx->bType)
            {
            case bt_compressed:
                rSize = ZSTDv08_decompressBlock_internal(dctx, dst, dstCapacity, src, srcSize);
                break;
            case bt_raw :
                rSize = ZSTDv08_copyRawBlock(dst, dstCapacity, src, srcSize);
                break;
            case bt_rle :
                rSize = ZSTDv08_setRleBlock(dst, dstCapacity, src, srcSize, dctx->rleSize);
                break;
            case bt_reserved :   /* should never happen */
            default:
                return ERROR(corruption_detected);
            }
            if (ZSTDv08_isError(rSize)) return rSize;
            if (dctx->fParams.checksumFlag) XXH64_update(&dctx->xxhState, dst, rSize);

            if (dctx->stage == ZSTDds_decompressLastBlock) {   /* end of frame */
                if (dctx->fParams.checksumFlag) {  /* another round for frame checksum */
                    dctx->expected = 4;
                    dctx->stage = ZSTDds_checkChecksum;
                } else {
                    dctx->expected = 0;   /* ends here */
                    dctx->stage = ZSTDds_getFrameHeaderSize;
                }
            } else {
                dctx->stage = ZSTDds_decodeBlockHeader;
                dctx->expected = ZSTDv08_blockHeaderSize;
                dctx->previousDstEnd = (char*)dst + rSize;
            }
            return rSize;
        }
    case ZSTDds_checkChecksum:
        {   U32 const h32 = (U32)XXH64_digest(&dctx->xxhState);
            U32 const check32 = MEM_readLE32(src);   /* srcSize == 4, guaranteed by dctx->expected */
            if (check32 != h32) return ERROR(checksum_wrong);
            dctx->expected = 0;
            dctx->stage = ZSTDds_getFrameHeaderSize;
            return 0;
        }
    case ZSTDds_decodeSkippableHeader:
        {   memcpy(dctx->headerBuffer + ZSTDv08_frameHeaderSize_min, src, dctx->expected);
            dctx->expected = MEM_readLE32(dctx->headerBuffer + 4);
            dctx->stage = ZSTDds_skipFrame;
            return 0;
        }
    case ZSTDds_skipFrame:
        {   dctx->expected = 0;
            dctx->stage = ZSTDds_getFrameHeaderSize;
            return 0;
        }
    default:
        return ERROR(GENERIC);   /* impossible */
    }
}


static size_t ZSTDv08_refDictContent(ZSTDv08_DCtx* dctx, const void* dict, size_t dictSize)
{
    dctx->dictEnd = dctx->previousDstEnd;
    dctx->vBase = (const char*)dict - ((const char*)(dctx->previousDstEnd) - (const char*)(dctx->base));
    dctx->base = dict;
    dctx->previousDstEnd = (const char*)dict + dictSize;
    return 0;
}

static size_t ZSTDv08_loadEntropy(ZSTDv08_DCtx* dctx, const void* const dict, size_t const dictSize)
{
    const BYTE* dictPtr = (const BYTE*)dict;
    const BYTE* const dictEnd = dictPtr + dictSize;

    {   size_t const hSize = HUFv08_readDTableX4(dctx->hufTable, dict, dictSize);
        if (HUFv08_isError(hSize)) return ERROR(dictionary_corrupted);
        dictPtr += hSize;
    }

    {   short offcodeNCount[MaxOff+1];
        U32 offcodeMaxValue=MaxOff, offcodeLog=OffFSELog;
        size_t const offcodeHeaderSize = FSEv08_readNCount(offcodeNCount, &offcodeMaxValue, &offcodeLog, dictPtr, dictEnd-dictPtr);
        if (FSEv08_isError(offcodeHeaderSize)) return ERROR(dictionary_corrupted);
        { size_t const errorCode = FSEv08_buildDTable(dctx->OffTable, offcodeNCount, offcodeMaxValue, offcodeLog);
          if (FSEv08_isError(errorCode)) return ERROR(dictionary_corrupted); }
        dictPtr += offcodeHeaderSize;
    }

    {   short matchlengthNCount[MaxML+1];
        unsigned matchlengthMaxValue = MaxML, matchlengthLog = MLFSELog;
        size_t const matchlengthHeaderSize = FSEv08_readNCount(matchlengthNCount, &matchlengthMaxValue, &matchlengthLog, dictPtr, dictEnd-dictPtr);
        if (FSEv08_isError(matchlengthHeaderSize)) return ERROR(dictionary_corrupted);
        { size_t const errorCode = FSEv08_buildDTable(dctx->MLTable, matchlengthNCount, matchlengthMaxValue, matchlengthLog);
          if (FSEv08_isError(errorCode)) return ERROR(dictionary_corrupted); }
        dictPtr += matchlengthHeaderSize;
    }

    {   short litlengthNCount[MaxLL+1];
        unsigned litlengthMaxValue = MaxLL, litlengthLog = LLFSELog;
        size_t const litlengthHeaderSize = FSEv08_readNCount(litlengthNCount, &litlengthMaxValue, &litlengthLog, dictPtr, dictEnd-dictPtr);
        if (FSEv08_isError(litlengthHeaderSize)) return ERROR(dictionary_corrupted);
        { size_t const errorCode = FSEv08_buildDTable(dctx->LLTable, litlengthNCount, litlengthMaxValue, litlengthLog);
          if (FSEv08_isError(errorCode)) return ERROR(dictionary_corrupted); }
        dictPtr += litlengthHeaderSize;
    }

    if (dictPtr+12 > dictEnd) return ERROR(dictionary_corrupted);
    dctx->rep[0] = MEM_readLE32(dictPtr+0); if (dctx->rep[0] >= dictSize) return ERROR(dictionary_corrupted);
    dctx->rep[1] = MEM_readLE32(dictPtr+4); if (dctx->rep[1] >= dictSize) return ERROR(dictionary_corrupted);
    dctx->rep[2] = MEM_readLE32(dictPtr+8); if (dctx->rep[2] >= dictSize) return ERROR(dictionary_corrupted);
    dictPtr += 12;

    dctx->litEntropy = dctx->fseEntropy = 1;
    return dictPtr - (const BYTE*)dict;
}

static size_t ZSTDv08_decompress_insertDictionary(ZSTDv08_DCtx* dctx, const void* dict, size_t dictSize)
{
    if (dictSize < 8) return ZSTDv08_refDictContent(dctx, dict, dictSize);
    {   U32 const magic = MEM_readLE32(dict);
        if (magic != ZSTDv08_DICT_MAGIC) {
            return ZSTDv08_refDictContent(dctx, dict, dictSize);   /* pure content mode */
    }   }
    dctx->dictID = MEM_readLE32((const char*)dict + 4);

    /* load entropy tables */
    dict = (const char*)dict + 8;
    dictSize -= 8;
    {   size_t const eSize = ZSTDv08_loadEntropy(dctx, dict, dictSize);
        if (ZSTDv08_isError(eSize)) return ERROR(dictionary_corrupted);
        dict = (const char*)dict + eSize;
        dictSize -= eSize;
    }

    /* reference dictionary content */
    return ZSTDv08_refDictContent(dctx, dict, dictSize);
}


size_t ZSTDv08_decompressBegin_usingDict(ZSTDv08_DCtx* dctx, const void* dict, size_t dictSize)
{
    { size_t const errorCode = ZSTDv08_decompressBegin(dctx);
      if (ZSTDv08_isError(errorCode)) return errorCode; }

    if (dict && dictSize) {
        size_t const errorCode = ZSTDv08_decompress_insertDictionary(dctx, dict, dictSize);
        if (ZSTDv08_isError(errorCode)) return ERROR(dictionary_corrupted);
    }

    return 0;
}


struct ZSTDv08_DDict_s {
    void* dict;
    size_t dictSize;
    ZSTDv08_DCtx* refContext;
};  /* typedef'd tp ZSTDv08_CDict within zstd.h */

ZSTDv08_DDict* ZSTDv08_createDDict_advanced(const void* dict, size_t dictSize, ZSTDv08_customMem customMem)
{
    if (!customMem.customAlloc && !customMem.customFree)
        customMem = defaultCustomMem;

    if (!customMem.customAlloc || !customMem.customFree)
        return NULL;

    {   ZSTDv08_DDict* const ddict = (ZSTDv08_DDict*) customMem.customAlloc(customMem.opaque, sizeof(*ddict));
        void* const dictContent = customMem.customAlloc(customMem.opaque, dictSize);
        ZSTDv08_DCtx* const dctx = ZSTDv08_createDCtx_advanced(customMem);

        if (!dictContent || !ddict || !dctx) {
            customMem.customFree(customMem.opaque, dictContent);
            customMem.customFree(customMem.opaque, ddict);
            customMem.customFree(customMem.opaque, dctx);
            return NULL;
        }

        memcpy(dictContent, dict, dictSize);
        {   size_t const errorCode = ZSTDv08_decompressBegin_usingDict(dctx, dictContent, dictSize);
            if (ZSTDv08_isError(errorCode)) {
                customMem.customFree(customMem.opaque, dictContent);
                customMem.customFree(customMem.opaque, ddict);
                customMem.customFree(customMem.opaque, dctx);
                return NULL;
        }   }

        ddict->dict = dictContent;
        ddict->dictSize = dictSize;
        ddict->refContext = dctx;
        return ddict;
    }
}

/*! ZSTDv08_createDDict() :
*   Create a digested dictionary, ready to start decompression without startup delay.
*   `dict` can be released after `ZSTDv08_DDict` creation */
ZSTDv08_DDict* ZSTDv08_createDDict(const void* dict, size_t dictSize)
{
    ZSTDv08_customMem const allocator = { NULL, NULL, NULL };
    return ZSTDv08_createDDict_advanced(dict, dictSize, allocator);
}

size_t ZSTDv08_freeDDict(ZSTDv08_DDict* ddict)
{
    ZSTDv08_freeFunction const cFree = ddict->refContext->customMem.customFree;
    void* const opaque = ddict->refContext->customMem.opaque;
    ZSTDv08_freeDCtx(ddict->refContext);
    cFree(opaque, ddict->dict);
    cFree(opaque, ddict);
    return 0;
}

/*! ZSTDv08_decompress_usingDDict() :
*   Decompression using a pre-digested Dictionary
*   Use dictionary without significant overhead. */
ZSTDLIB_API size_t ZSTDv08_decompress_usingDDict(ZSTDv08_DCtx* dctx,
                                           void* dst, size_t dstCapacity,
                                     const void* src, size_t srcSize,
                                     const ZSTDv08_DDict* ddict)
{
    return ZSTDv08_decompress_usingPreparedDCtx(dctx, ddict->refContext,
                                           dst, dstCapacity,
                                           src, srcSize);
}
/*
    Buffered version of Zstd compression library
    Copyright (C) 2015-2016, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - zstd homepage : http://www.zstd.net/
*/


/*-***************************************************************************
*  Streaming decompression howto
*
*  A ZBUFFv08_DCtx object is required to track streaming operations.
*  Use ZBUFFv08_createDCtx() and ZBUFFv08_freeDCtx() to create/release resources.
*  Use ZBUFFv08_decompressInit() to start a new decompression operation,
*   or ZBUFFv08_decompressInitDictionary() if decompression requires a dictionary.
*  Note that ZBUFFv08_DCtx objects can be re-init multiple times.
*
*  Use ZBUFFv08_decompressContinue() repetitively to consume your input.
*  *srcSizePtr and *dstCapacityPtr can be any size.
*  The function will report how many bytes were read or written by modifying *srcSizePtr and *dstCapacityPtr.
*  Note that it may not consume the entire input, in which case it's up to the caller to present remaining input again.
*  The content of @dst will be overwritten (up to *dstCapacityPtr) at each function call, so save its content if it matters, or change @dst.
*  @return : a hint to preferred nb of bytes to use as input for next function call (it's only a hint, to help latency),
*            or 0 when a frame is completely decoded,
*            or an error code, which can be tested using ZBUFFv08_isError().
*
*  Hint : recommended buffer sizes (not compulsory) : ZBUFFv08_recommendedDInSize() and ZBUFFv08_recommendedDOutSize()
*  output : ZBUFFv08_recommendedDOutSize==128 KB block size is the internal unit, it ensures it's always possible to write a full block when decoded.
*  input  : ZBUFFv08_recommendedDInSize == 128KB + 3;
*           just follow indications from ZBUFFv08_decompressContinue() to minimize latency. It should always be <= 128 KB + 3 .
* *******************************************************************************/

typedef enum { ZBUFFds_init, ZBUFFds_loadHeader,
               ZBUFFds_read, ZBUFFds_load, ZBUFFds_flush } ZBUFFv08_dStage;

/* *** Resource management *** */
struct ZBUFFv08_DCtx_s {
    ZSTDv08_DCtx* zd;
    ZSTDv08_frameParams fParams;
    ZBUFFv08_dStage stage;
    char*  inBuff;
    size_t inBuffSize;
    size_t inPos;
    char*  outBuff;
    size_t outBuffSize;
    size_t outStart;
    size_t outEnd;
    size_t blockSize;
    BYTE headerBuffer[ZSTDv08_FRAMEHEADERSIZE_MAX];
    size_t lhSize;
    ZSTDv08_customMem customMem;
};   /* typedef'd to ZBUFFv08_DCtx within "zstd_buffered.h" */


ZBUFFv08_DCtx* ZBUFFv08_createDCtx(void)
{
    return ZBUFFv08_createDCtx_advanced(defaultCustomMem);
}

ZBUFFv08_DCtx* ZBUFFv08_createDCtx_advanced(ZSTDv08_customMem customMem)
{
    ZBUFFv08_DCtx* zbd;

    if (!customMem.customAlloc && !customMem.customFree)
        customMem = defaultCustomMem;

    if (!customMem.customAlloc || !customMem.customFree)
        return NULL;

    zbd = (ZBUFFv08_DCtx*)customMem.customAlloc(customMem.opaque, sizeof(ZBUFFv08_DCtx));
    if (zbd==NULL) return NULL;
    memset(zbd, 0, sizeof(ZBUFFv08_DCtx));
    memcpy(&zbd->customMem, &customMem, sizeof(ZSTDv08_customMem));
    zbd->zd = ZSTDv08_createDCtx_advanced(customMem);
    if (zbd->zd == NULL) { ZBUFFv08_freeDCtx(zbd); return NULL; }
    zbd->stage = ZBUFFds_init;
    return zbd;
}

size_t ZBUFFv08_freeDCtx(ZBUFFv08_DCtx* zbd)
{
    if (zbd==NULL) return 0;   /* support free on null */
    ZSTDv08_freeDCtx(zbd->zd);
    if (zbd->inBuff) zbd->customMem.customFree(zbd->customMem.opaque, zbd->inBuff);
    if (zbd->outBuff) zbd->customMem.customFree(zbd->customMem.opaque, zbd->outBuff);
    zbd->customMem.customFree(zbd->customMem.opaque, zbd);
    return 0;
}


/* *** Initialization *** */

size_t ZBUFFv08_decompressInitDictionary(ZBUFFv08_DCtx* zbd, const void* dict, size_t dictSize)
{
    zbd->stage = ZBUFFds_loadHeader;
    zbd->lhSize = zbd->inPos = zbd->outStart = zbd->outEnd = 0;
    return ZSTDv08_decompressBegin_usingDict(zbd->zd, dict, dictSize);
}

size_t ZBUFFv08_decompressInit(ZBUFFv08_DCtx* zbd)
{
    return ZBUFFv08_decompressInitDictionary(zbd, NULL, 0);
}


/* internal util function */
MEM_STATIC size_t ZBUFFv08_limitCopy(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    size_t const length = MIN(dstCapacity, srcSize);
    memcpy(dst, src, length);
    return length;
}


/* *** Decompression *** */

size_t ZBUFFv08_decompressContinue(ZBUFFv08_DCtx* zbd,
                                void* dst, size_t* dstCapacityPtr,
                          const void* src, size_t* srcSizePtr)
{
    const char* const istart = (const char*)src;
    const char* const iend = istart + *srcSizePtr;
    const char* ip = istart;
    char* const ostart = (char*)dst;
    char* const oend = ostart + *dstCapacityPtr;
    char* op = ostart;
    U32 someMoreWork = 1;

    while (someMoreWork) {
        switch(zbd->stage)
        {
        case ZBUFFds_init :
            return ERROR(init_missing);

        case ZBUFFds_loadHeader :
            {   size_t const hSize = ZSTDv08_getFrameParams(&(zbd->fParams), zbd->headerBuffer, zbd->lhSize);
                if (ZSTDv08_isError(hSize)) return hSize;
                if (hSize != 0) {   /* need more input */
                    size_t const toLoad = hSize - zbd->lhSize;   /* if hSize!=0, hSize > zbd->lhSize */
                    if (toLoad > (size_t)(iend-ip)) {   /* not enough input to load full header */
                        memcpy(zbd->headerBuffer + zbd->lhSize, ip, iend-ip);
                        zbd->lhSize += iend-ip;
                        *dstCapacityPtr = 0;
                        return (hSize - zbd->lhSize) + ZSTDv08_blockHeaderSize;   /* remaining header bytes + next block header */
                    }
                    memcpy(zbd->headerBuffer + zbd->lhSize, ip, toLoad); zbd->lhSize = hSize; ip += toLoad;
                    break;
            }   }

            /* Consume header */
            {   size_t const h1Size = ZSTDv08_nextSrcSizeToDecompress(zbd->zd);  /* == ZSTDv08_frameHeaderSize_min */
                size_t const h1Result = ZSTDv08_decompressContinue(zbd->zd, NULL, 0, zbd->headerBuffer, h1Size);
                if (ZSTDv08_isError(h1Result)) return h1Result;   /* should not happen : already checked */
                if (h1Size < zbd->lhSize) {   /* long header */
                    size_t const h2Size = ZSTDv08_nextSrcSizeToDecompress(zbd->zd);
                    size_t const h2Result = ZSTDv08_decompressContinue(zbd->zd, NULL, 0, zbd->headerBuffer+h1Size, h2Size);
                    if (ZSTDv08_isError(h2Result)) return h2Result;
            }   }

            zbd->fParams.windowSize = MAX(zbd->fParams.windowSize, 1U << ZSTDv08_WINDOWLOG_ABSOLUTEMIN);

            /* Frame header instruct buffer sizes */
            {   size_t const blockSize = MIN(zbd->fParams.windowSize, ZSTDv08_BLOCKSIZE_ABSOLUTEMAX);
                size_t const neededOutSize = zbd->fParams.windowSize + blockSize;
                zbd->blockSize = blockSize;
                if (zbd->inBuffSize < blockSize) {
                    zbd->customMem.customFree(zbd->customMem.opaque, zbd->inBuff);
                    zbd->inBuffSize = blockSize;
                    zbd->inBuff = (char*)zbd->customMem.customAlloc(zbd->customMem.opaque, blockSize);
                    if (zbd->inBuff == NULL) return ERROR(memory_allocation);
                }
                if (zbd->outBuffSize < neededOutSize) {
                    zbd->customMem.customFree(zbd->customMem.opaque, zbd->outBuff);
                    zbd->outBuffSize = neededOutSize;
                    zbd->outBuff = (char*)zbd->customMem.customAlloc(zbd->customMem.opaque, neededOutSize);
                    if (zbd->outBuff == NULL) return ERROR(memory_allocation);
            }   }
            zbd->stage = ZBUFFds_read;
            /* pass-through */

        case ZBUFFds_read:
            {   size_t const neededInSize = ZSTDv08_nextSrcSizeToDecompress(zbd->zd);
                if (neededInSize==0) {  /* end of frame */
                    zbd->stage = ZBUFFds_init;
                    someMoreWork = 0;
                    break;
                }
                if ((size_t)(iend-ip) >= neededInSize) {  /* decode directly from src */
                    const int isSkipFrame = ZSTDv08_isSkipFrame(zbd->zd);
                    size_t const decodedSize = ZSTDv08_decompressContinue(zbd->zd,
                        zbd->outBuff + zbd->outStart, (isSkipFrame ? 0 : zbd->outBuffSize - zbd->outStart),
                        ip, neededInSize);
                    if (ZSTDv08_isError(decodedSize)) return decodedSize;
                    ip += neededInSize;
                    if (!decodedSize && !isSkipFrame) break;   /* this was just a header */
                    zbd->outEnd = zbd->outStart +  decodedSize;
                    zbd->stage = ZBUFFds_flush;
                    break;
                }
                if (ip==iend) { someMoreWork = 0; break; }   /* no more input */
                zbd->stage = ZBUFFds_load;
                /* pass-through */
            }

        case ZBUFFds_load:
            {   size_t const neededInSize = ZSTDv08_nextSrcSizeToDecompress(zbd->zd);
                size_t const toLoad = neededInSize - zbd->inPos;   /* should always be <= remaining space within inBuff */
                size_t loadedSize;
                if (toLoad > zbd->inBuffSize - zbd->inPos) return ERROR(corruption_detected);   /* should never happen */
                loadedSize = ZBUFFv08_limitCopy(zbd->inBuff + zbd->inPos, toLoad, ip, iend-ip);
                ip += loadedSize;
                zbd->inPos += loadedSize;
                if (loadedSize < toLoad) { someMoreWork = 0; break; }   /* not enough input, wait for more */

                /* decode loaded input */
                {  const int isSkipFrame = ZSTDv08_isSkipFrame(zbd->zd);
                   size_t const decodedSize = ZSTDv08_decompressContinue(zbd->zd,
                        zbd->outBuff + zbd->outStart, zbd->outBuffSize - zbd->outStart,
                        zbd->inBuff, neededInSize);
                    if (ZSTDv08_isError(decodedSize)) return decodedSize;
                    zbd->inPos = 0;   /* input is consumed */
                    if (!decodedSize && !isSkipFrame) { zbd->stage = ZBUFFds_read; break; }   /* this was just a header */
                    zbd->outEnd = zbd->outStart +  decodedSize;
                    zbd->stage = ZBUFFds_flush;
                    /* pass-through */
            }   }

        case ZBUFFds_flush:
            {   size_t const toFlushSize = zbd->outEnd - zbd->outStart;
                size_t const flushedSize = ZBUFFv08_limitCopy(op, oend-op, zbd->outBuff + zbd->outStart, toFlushSize);
                op += flushedSize;
                zbd->outStart += flushedSize;
                if (flushedSize == toFlushSize) {  /* flush completed */
                    zbd->stage = ZBUFFds_read;
                    if (zbd->outStart + zbd->blockSize > zbd->outBuffSize)
                        zbd->outStart = zbd->outEnd = 0;
                    break;
                }
                /* cannot flush everything */
                someMoreWork = 0;
                break;
            }
        default: return ERROR(GENERIC);   /* impossible */
    }   }

    /* result */
    *srcSizePtr = ip-istart;
    *dstCapacityPtr = op-ostart;
    {   size_t nextSrcSizeHint = ZSTDv08_nextSrcSizeToDecompress(zbd->zd);
        if (!nextSrcSizeHint) return (zbd->outEnd != zbd->outStart);   /* return 0 only if fully flushed too */
        nextSrcSizeHint += ZSTDv08_blockHeaderSize * (ZSTDv08_nextInputType(zbd->zd) == ZSTDnit_block);
        if (zbd->inPos > nextSrcSizeHint) return ERROR(GENERIC);   /* should never happen */
        nextSrcSizeHint -= zbd->inPos;   /* already loaded*/
        return nextSrcSizeHint;
    }
}


/* *************************************
*  Tool functions
***************************************/
size_t ZBUFFv08_recommendedDInSize(void)  { return ZSTDv08_BLOCKSIZE_ABSOLUTEMAX + ZSTDv08_blockHeaderSize /* block header size*/ ; }
size_t ZBUFFv08_recommendedDOutSize(void) { return ZSTDv08_BLOCKSIZE_ABSOLUTEMAX; }
