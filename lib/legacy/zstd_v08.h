/*
    zstd_v08 - decoder for 0.8 format
    Header File
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
    - zstd source repository : https://github.com/Cyan4973/zstd
*/
#ifndef ZSTDv08_H_235446
#define ZSTDv08_H_235446

#if defined (__cplusplus)
extern "C" {
#endif

/*======  Dependency  ======*/
#include <stddef.h>   /* size_t */


/*======  Export for Windows  ======*/
/*!
*  ZSTDv08_DLL_EXPORT :
*  Enable exporting of functions when building a Windows DLL
*/
#if defined(_WIN32) && defined(ZSTDv08_DLL_EXPORT) && (ZSTDv08_DLL_EXPORT==1)
#  define ZSTDLIB_API __declspec(dllexport)
#else
#  define ZSTDLIB_API
#endif


/* *************************************
*  Simple API
***************************************/
/*! ZSTDv08_getDecompressedSize() :
*   @return : decompressed size as a 64-bits value _if known_, 0 otherwise.
*    note 1 : decompressed size can be very large (64-bits value),
*             potentially larger than what local system can handle as a single memory segment.
*             In which case, it's necessary to use streaming mode to decompress data.
*    note 2 : decompressed size is an optional field, that may not be present.
*             When `return==0`, consider data to decompress could have any size.
*             In which case, it's necessary to use streaming mode to decompress data,
*             or rely on application's implied limits.
*             (For example, it may know that its own data is necessarily cut into blocks <= 16 KB).
*    note 3 : decompressed size could be wrong or intentionally modified !
*             Always ensure result fits within application's authorized limits !
*             Each application can have its own set of conditions.
*             If the intention is to decompress public data compressed by zstd command line utility,
*             it is recommended to support at least 8 MB for extended compatibility.
*    note 4 : when `return==0`, if precise failure cause is needed, use ZSTDv08_getFrameParams() to know more. */
unsigned long long ZSTDv08_getDecompressedSize(const void* src, size_t srcSize);

/*! ZSTDv08_decompress() :
    `compressedSize` : must be the _exact_ size of compressed input, otherwise decompression will fail.
    `dstCapacity` must be equal or larger than originalSize (see ZSTDv08_getDecompressedSize() ).
    If originalSize is unknown, and if there is no implied application-specific limitations,
    it's necessary to use streaming mode to decompress data.
    @return : the number of bytes decompressed into `dst` (<= `dstCapacity`),
              or an errorCode if it fails (which can be tested using ZSTDv08_isError()) */
ZSTDLIB_API size_t ZSTDv08_decompress( void* dst, size_t dstCapacity,
                              const void* src, size_t compressedSize);


/*======  Helper functions  ======*/
ZSTDLIB_API unsigned    ZSTDv08_isError(size_t code);          /*!< tells if a `size_t` function result is an error code */
ZSTDLIB_API const char* ZSTDv08_getErrorName(size_t code);     /*!< provides readable string from an error code */


/*-*************************************
*  Explicit memory management
***************************************/
/** Decompression context */
typedef struct ZSTDv08_DCtx_s ZSTDv08_DCtx;                       /*< incomplete type */
ZSTDLIB_API ZSTDv08_DCtx* ZSTDv08_createDCtx(void);
ZSTDLIB_API size_t     ZSTDv08_freeDCtx(ZSTDv08_DCtx* dctx);

/** ZSTDv08_decompressDCtx() :
*   Same as ZSTDv08_decompress(), requires an allocated ZSTDv08_DCtx (see ZSTDv08_createDCtx()) */
ZSTDLIB_API size_t ZSTDv08_decompressDCtx(ZSTDv08_DCtx* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);


/*-************************
*  Simple dictionary API
***************************/
/*! ZSTDv08_decompress_usingDict() :
*   Decompression using a predefined Dictionary (see dictBuilder/zdict.h).
*   Dictionary must be identical to the one used during compression.
*   Note : This function load the dictionary, resulting in a significant startup time */
ZSTDLIB_API size_t ZSTDv08_decompress_usingDict(ZSTDv08_DCtx* dctx,
                                             void* dst, size_t dstCapacity,
                                       const void* src, size_t srcSize,
                                       const void* dict,size_t dictSize);


/*-**************************
*  Fast Dictionary API
****************************/
/*! ZSTDv08_createDDict() :
*   Create a digested dictionary, ready to start decompression operation without startup delay.
*   `dict` can be released after creation */
typedef struct ZSTDv08_DDict_s ZSTDv08_DDict;
ZSTDLIB_API ZSTDv08_DDict* ZSTDv08_createDDict(const void* dict, size_t dictSize);
ZSTDLIB_API size_t      ZSTDv08_freeDDict(ZSTDv08_DDict* ddict);

/*! ZSTDv08_decompress_usingDDict() :
*   Decompression using a digested Dictionary
*   Faster startup than ZSTDv08_decompress_usingDict(), recommended when same dictionary is used multiple times. */
ZSTDLIB_API size_t ZSTDv08_decompress_usingDDict(ZSTDv08_DCtx* dctx,
                                              void* dst, size_t dstCapacity,
                                        const void* src, size_t srcSize,
                                        const ZSTDv08_DDict* ddict);

typedef struct {
    unsigned long long frameContentSize;
    unsigned windowSize;
    unsigned dictID;
    unsigned checksumFlag;
} ZSTDv08_frameParams;

ZSTDLIB_API size_t ZSTDv08_getFrameParams(ZSTDv08_frameParams* fparamsPtr, const void* src, size_t srcSize);   /**< doesn't consume input, see details below */


/* ***************************************************************
*  Compiler specifics
*****************************************************************/
/* ZSTDv08_DLL_EXPORT :
*  Enable exporting of functions when building a Windows DLL */
#if defined(_WIN32) && defined(ZSTDv08_DLL_EXPORT) && (ZSTDv08_DLL_EXPORT==1)
#  define ZSTDLIB_API __declspec(dllexport)
#else
#  define ZSTDLIB_API
#endif


/* *************************************
*  Streaming functions
***************************************/
/* This is the easier "buffered" streaming API,
*  using an internal buffer to lift all restrictions on user-provided buffers
*  which can be any size, any place, for both input and output.
*  ZBUFF and ZSTD are 100% interoperable,
*  frames created by one can be decoded by the other one */

typedef struct ZBUFFv08_DCtx_s ZBUFFv08_DCtx;
ZSTDLIB_API ZBUFFv08_DCtx* ZBUFFv08_createDCtx(void);
ZSTDLIB_API size_t      ZBUFFv08_freeDCtx(ZBUFFv08_DCtx* dctx);

ZSTDLIB_API size_t ZBUFFv08_decompressInit(ZBUFFv08_DCtx* dctx);
ZSTDLIB_API size_t ZBUFFv08_decompressInitDictionary(ZBUFFv08_DCtx* dctx, const void* dict, size_t dictSize);

ZSTDLIB_API size_t ZBUFFv08_decompressContinue(ZBUFFv08_DCtx* dctx,
                                            void* dst, size_t* dstCapacityPtr,
                                      const void* src, size_t* srcSizePtr);

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
*  The content of `dst` will be overwritten (up to *dstCapacityPtr) at each function call, so save its content if it matters, or change `dst`.
*  @return : 0 when a frame is completely decoded and fully flushed,
*            1 when there is still some data left within internal buffer to flush,
*            >1 when more data is expected, with value being a suggested next input size (it's just a hint, which helps latency),
*            or an error code, which can be tested using ZBUFFv08_isError().
*
*  Hint : recommended buffer sizes (not compulsory) : ZBUFFv08_recommendedDInSize() and ZBUFFv08_recommendedDOutSize()
*  output : ZBUFFv08_recommendedDOutSize== 128 KB block size is the internal unit, it ensures it's always possible to write a full block when decoded.
*  input  : ZBUFFv08_recommendedDInSize == 128KB + 3;
*           just follow indications from ZBUFFv08_decompressContinue() to minimize latency. It should always be <= 128 KB + 3 .
* *******************************************************************************/


/* *************************************
*  Tool functions
***************************************/
ZSTDLIB_API unsigned ZBUFFv08_isError(size_t errorCode);
ZSTDLIB_API const char* ZBUFFv08_getErrorName(size_t errorCode);

/** Functions below provide recommended buffer sizes for Compression or Decompression operations.
*   These sizes are just hints, they tend to offer better latency */
ZSTDLIB_API size_t ZBUFFv08_recommendedDInSize(void);
ZSTDLIB_API size_t ZBUFFv08_recommendedDOutSize(void);



#define ZSTDv08_MAGICNUMBER            0xFD2FB528   /* v0.8 */


#if defined (__cplusplus)
}
#endif

#endif  /* ZSTDv08_H_235446 */

