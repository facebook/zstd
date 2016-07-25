/*
    zstd_legacy - decoder for legacy format
    Header File
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
    - zstd source repository : https://github.com/Cyan4973/zstd
    - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/
#ifndef ZSTD_LEGACY_H
#define ZSTD_LEGACY_H

#if defined (__cplusplus)
extern "C" {
#endif

/* *************************************
*  Includes
***************************************/
#include "mem.h"            /* MEM_STATIC */
#include "error_private.h"  /* ERROR */
#include "zstd_v01.h"
#include "zstd_v02.h"
#include "zstd_v03.h"
#include "zstd_v04.h"
#include "zstd_v05.h"
#include "zstd_v06.h"
#include "zstd_v07.h"


/** ZSTD_isLegacy() :
    @return : > 0 if supported by legacy decoder. 0 otherwise.
              return value is the version.
*/
MEM_STATIC unsigned ZSTD_isLegacy(const void* src, size_t srcSize)
{
    U32 magicNumberLE;
    if (srcSize<4) return 0;
    magicNumberLE = MEM_readLE32(src);
    switch(magicNumberLE)
    {
        case ZSTDv01_magicNumberLE:return 1;
        case ZSTDv02_magicNumber : return 2;
        case ZSTDv03_magicNumber : return 3;
        case ZSTDv04_magicNumber : return 4;
        case ZSTDv05_MAGICNUMBER : return 5;
        case ZSTDv06_MAGICNUMBER : return 6;
        case ZSTDv07_MAGICNUMBER : return 7;
        default : return 0;
    }
}


MEM_STATIC unsigned long long ZSTD_getDecompressedSize_legacy(const void* src, size_t srcSize)
{
    if (srcSize < 4) return 0;

    {   U32 const version = ZSTD_isLegacy(src, srcSize);
        if (version < 5) return 0;  /* no decompressed size in frame header, or not a legacy format */
        if (version==5) {
            ZSTDv05_parameters fParams;
            size_t const frResult = ZSTDv05_getFrameParams(&fParams, src, srcSize);
            if (frResult != 0) return 0;
            return fParams.srcSize;
        }
        if (version==6) {
            ZSTDv06_frameParams fParams;
            size_t const frResult = ZSTDv06_getFrameParams(&fParams, src, srcSize);
            if (frResult != 0) return 0;
            return fParams.frameContentSize;
        }
        if (version==7) {
            ZSTDv07_frameParams fParams;
            size_t const frResult = ZSTDv07_getFrameParams(&fParams, src, srcSize);
            if (frResult != 0) return 0;
            return fParams.frameContentSize;
        }
        return 0;   /* should not be possible */
    }
}

MEM_STATIC size_t ZSTD_decompressLegacy(
                     void* dst, size_t dstCapacity,
               const void* src, size_t compressedSize,
               const void* dict,size_t dictSize)
{
    U32 const version = ZSTD_isLegacy(src, compressedSize);
    switch(version)
    {
        case 1 :
            return ZSTDv01_decompress(dst, dstCapacity, src, compressedSize);
        case 2 :
            return ZSTDv02_decompress(dst, dstCapacity, src, compressedSize);
        case 3 :
            return ZSTDv03_decompress(dst, dstCapacity, src, compressedSize);
        case 4 :
            return ZSTDv04_decompress(dst, dstCapacity, src, compressedSize);
        case 5 :
            {   size_t result;
                ZSTDv05_DCtx* const zd = ZSTDv05_createDCtx();
                if (zd==NULL) return ERROR(memory_allocation);
                result = ZSTDv05_decompress_usingDict(zd, dst, dstCapacity, src, compressedSize, dict, dictSize);
                ZSTDv05_freeDCtx(zd);
                return result;
            }
        case 6 :
            {   size_t result;
                ZSTDv06_DCtx* const zd = ZSTDv06_createDCtx();
                if (zd==NULL) return ERROR(memory_allocation);
                result = ZSTDv06_decompress_usingDict(zd, dst, dstCapacity, src, compressedSize, dict, dictSize);
                ZSTDv06_freeDCtx(zd);
                return result;
            }
        case 7 :
            {   size_t result;
                ZSTDv07_DCtx* const zd = ZSTDv07_createDCtx();
                if (zd==NULL) return ERROR(memory_allocation);
                result = ZSTDv07_decompress_usingDict(zd, dst, dstCapacity, src, compressedSize, dict, dictSize);
                ZSTDv07_freeDCtx(zd);
                return result;
            }
        default :
            return ERROR(prefix_unknown);
    }
}



#if defined (__cplusplus)
}
#endif

#endif   /* ZSTD_LEGACY_H */
