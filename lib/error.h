/* ******************************************************************
   Error codes and messages
   Copyright (C) 2013-2015, Yann Collet

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
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef ERROR_H_MODULE
#define ERROR_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif


/******************************************
*  Includes
******************************************/
#include <stddef.h>    /* size_t, ptrdiff_t */


/******************************************
*  Compiler-specific
******************************************/
#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#  define ERR_STATIC static inline
#elif defined(_MSC_VER)
#  define ERR_STATIC static __inline
#elif defined(__GNUC__)
#  define ERR_STATIC static __attribute__((unused))
#else
#  define ERR_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif


/******************************************
*  Error Management
******************************************/
#define PREFIX(name) ZSTD_error_##name

#ifdef ERROR
#  undef ERROR   /* reported already defined on VS 2015 by Rich Geldreich */
#endif
#define ERROR(name) (size_t)-PREFIX(name)

#define ERROR_LIST(ITEM) \
        ITEM(PREFIX(No_Error)) ITEM(PREFIX(GENERIC)) \
        ITEM(PREFIX(prefix_unknown)) ITEM(PREFIX(frameParameter_unsupported)) ITEM(PREFIX(frameParameter_unsupportedBy32bitsImplementation)) \
        ITEM(PREFIX(init_missing)) ITEM(PREFIX(memory_allocation)) ITEM(PREFIX(stage_wrong)) \
        ITEM(PREFIX(dstSize_tooSmall)) ITEM(PREFIX(srcSize_wrong)) \
        ITEM(PREFIX(corruption_detected)) \
        ITEM(PREFIX(tableLog_tooLarge)) ITEM(PREFIX(maxSymbolValue_tooLarge)) ITEM(PREFIX(maxSymbolValue_tooSmall)) \
        ITEM(PREFIX(maxCode))

#define ERROR_GENERATE_ENUM(ENUM) ENUM,
typedef enum { ERROR_LIST(ERROR_GENERATE_ENUM) } ERR_codes;  /* enum is exposed, to detect & handle specific errors; compare function result to -enum value */

#define ERROR_CONVERTTOSTRING(STRING) #STRING,
#define ERROR_GENERATE_STRING(EXPR) ERROR_CONVERTTOSTRING(EXPR)
static const char* ERR_strings[] = { ERROR_LIST(ERROR_GENERATE_STRING) };

ERR_STATIC unsigned ERR_isError(size_t code) { return (code > ERROR(maxCode)); }

ERR_STATIC const char* ERR_getErrorName(size_t code)
{
    static const char* codeError = "Unspecified error code";
    if (ERR_isError(code)) return ERR_strings[-(int)(code)];
    return codeError;
}


#if defined (__cplusplus)
}
#endif

#endif /* ERROR_H_MODULE */
