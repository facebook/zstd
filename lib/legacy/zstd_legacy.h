/*
    zstd_v02 - decoder for 0.2 format
    Header File
    Copyright (C) 2015, Yann Collet.

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
#include "mem.h"        /* MEM_STATIC */
#include "error.h"      /* ERROR */
#include "zstd_v01.h"
#include "zstd_v02.h"

MEM_STATIC unsigned ZSTD_isLegacy (U32 magicNumberLE)
{
	switch(magicNumberLE)
	{
		case ZSTDv01_magicNumberLE :
		case ZSTDv02_magicNumber : return 1;
		default : return 0;
	}
}


MEM_STATIC size_t ZSTD_decompressLegacy(
                     void* dst, size_t maxOriginalSize,
               const void* src, size_t compressedSize,
                     U32 magicNumberLE)
{
	switch(magicNumberLE)
	{
		case ZSTDv01_magicNumberLE :
			return ZSTDv01_decompress(dst, maxOriginalSize, src, compressedSize);
		case ZSTDv02_magicNumber :
			return ZSTDv02_decompress(dst, maxOriginalSize, src, compressedSize);
		default :
		    return ERROR(prefix_unknown);
	}
}



#if defined (__cplusplus)
}
#endif

#endif   /* ZSTD_LEGACY_H */
