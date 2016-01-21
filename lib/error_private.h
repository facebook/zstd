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
   - Source repository : https://github.com/Cyan4973/zstd
****************************************************************** */
/* Note : this module is expected to remain private, do not expose it */

#ifndef ERROR_H_MODULE
#define ERROR_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif


/* *****************************************
*  Includes
******************************************/
#include <stddef.h>        /* size_t, ptrdiff_t */
#include "error_public.h"  /* enum list */


/* *****************************************
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


/* *****************************************
*  Error Codes
******************************************/
#define PREFIX(name) ZSTD_error_##name

#ifdef ERROR
#  undef ERROR   /* reported already defined on VS 2015 by Rich Geldreich */
#endif
#define ERROR(name) (size_t)-PREFIX(name)

ERR_STATIC unsigned ERR_isError(size_t code) { return (code > ERROR(maxCode)); }


/* *****************************************
*  Error Strings
******************************************/

ERR_STATIC const char* ERR_getErrorName(size_t code)
{
    static const char* codeError = "Unspecified error code";
    switch( (size_t)-code )
    {
    case ZSTD_error_No_Error: return "No error detected";
    case ZSTD_error_GENERIC:  return "Error (generic)";
    case ZSTD_error_prefix_unknown: return "Unknown frame descriptor";
    case ZSTD_error_frameParameter_unsupported: return "Unsupported frame parameter";
    case ZSTD_error_frameParameter_unsupportedBy32bitsImplementation: return "Frame parameter unsupported in 32-bits mode";
    case ZSTD_error_init_missing: return "Context should be init first";
    case ZSTD_error_memory_allocation: return "Allocation error : not enough memory";
    case ZSTD_error_dstSize_tooSmall: return "Destination buffer is too small";
    case ZSTD_error_srcSize_wrong: return "Src size incorrect";
    case ZSTD_error_corruption_detected: return "Corrupted block detected";
    case ZSTD_error_tableLog_tooLarge: return "tableLog requires too much memory";
    case ZSTD_error_maxSymbolValue_tooLarge: return "Unsupported max possible Symbol Value : too large";
    case ZSTD_error_maxSymbolValue_tooSmall: return "Specified maxSymbolValue is too small";
    case ZSTD_error_maxCode:
    default: return codeError;
    }
}


#if defined (__cplusplus)
}
#endif

#endif /* ERROR_H_MODULE */
