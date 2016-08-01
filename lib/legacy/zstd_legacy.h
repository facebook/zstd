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
#include "zstd_v07.h"
#include "zstd_v08.h"


/** ZSTD_isLegacy() :
    @return : > 0 if supported by legacy decoder. 0 otherwise.
              return value is the version.
*/
MEM_STATIC unsigned ZSTD_isLegacy (U32 magicNumberLE)
{
	switch(magicNumberLE)
	{
		case ZSTDv01_magicNumberLE:return 1;
		case ZSTDv02_magicNumber : return 2;
		case ZSTDv03_magicNumber : return 3;
		case ZSTDv04_magicNumber : return 4;
		case ZSTDv05_MAGICNUMBER : return 5;
		case ZSTDv07_MAGICNUMBER : return 7;
		case ZSTDv08_MAGICNUMBER : return 8;
		default : return 0;
	}
}


MEM_STATIC size_t ZSTD_decompressLegacy(
                     void* dst, size_t dstCapacity,
               const void* src, size_t compressedSize,
               const void* dict,size_t dictSize,
                     U32 magicNumberLE)
{
    switch(magicNumberLE)
    {
        case ZSTDv01_magicNumberLE :
            return ZSTDv01_decompress(dst, dstCapacity, src, compressedSize);
        case ZSTDv02_magicNumber :
            return ZSTDv02_decompress(dst, dstCapacity, src, compressedSize);
        case ZSTDv03_magicNumber :
            return ZSTDv03_decompress(dst, dstCapacity, src, compressedSize);
        case ZSTDv04_magicNumber :
            return ZSTDv04_decompress(dst, dstCapacity, src, compressedSize);
        case ZSTDv05_MAGICNUMBER :
            {
                size_t result;
                ZSTDv05_DCtx* zd = ZSTDv05_createDCtx();
                if (zd==NULL) return ERROR(memory_allocation);
                result = ZSTDv05_decompress_usingDict(zd, dst, dstCapacity, src, compressedSize, dict, dictSize);
                ZSTDv05_freeDCtx(zd);
                return result;
            }
        case ZSTDv07_MAGICNUMBER :
            {   size_t result;
                ZSTDv07_DCtx* const zd = ZSTDv07_createDCtx();
                if (zd==NULL) return ERROR(memory_allocation);
                result = ZSTDv07_decompress_usingDict(zd, dst, dstCapacity, src, compressedSize, dict, dictSize);
                ZSTDv07_freeDCtx(zd);
                return result;
            }
        case ZSTDv08_MAGICNUMBER :
            {   size_t result;
                ZSTDv08_DCtx* const zd = ZSTDv08_createDCtx();
                if (zd==NULL) return ERROR(memory_allocation);
                result = ZSTDv08_decompress_usingDict(zd, dst, dstCapacity, src, compressedSize, dict, dictSize);
                ZSTDv08_freeDCtx(zd);
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
