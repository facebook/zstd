
/* ===   Dependencies   === */
#include <stddef.h>   /* size_t */
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_parameters */
#include "zstd.h"     /* ZSTD_inBuffer, ZSTD_outBuffer */


/* ===   Simple one-pass functions   === */

typedef struct ZSTDMT_CCtx_s ZSTDMT_CCtx;
ZSTDMT_CCtx* ZSTDMT_createCCtx(unsigned nbThreads);
size_t ZSTDMT_freeCCtx(ZSTDMT_CCtx* cctx);

size_t ZSTDMT_compressCCtx(ZSTDMT_CCtx* cctx,
                           void* dst, size_t dstCapacity,
                     const void* src, size_t srcSize,
                           int compressionLevel);


/* ===   Streaming functions   === */

size_t ZSTDMT_initCStream(ZSTDMT_CCtx* zcs, int compressionLevel);
size_t ZSTDMT_resetCStream(ZSTDMT_CCtx* zcs, unsigned long long pledgedSrcSize);   /**< pledgedSrcSize is optional and can be zero == unknown */
size_t ZSTDMT_initCStream_advanced(ZSTDMT_CCtx* zcs, const void* dict, size_t dictSize,
                                   ZSTD_parameters params, unsigned long long pledgedSrcSize);  /**< pledgedSrcSize is optional and can be zero == unknown ; current limitation : no checksum */

size_t ZSTDMT_compressStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output, ZSTD_inBuffer* input);

size_t ZSTDMT_flushStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output);   /**< @return : 0 == all flushed; >0 : still some data to be flushed; or an error code (ZSTD_isError()) */
size_t ZSTDMT_endStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output);     /**< @return : 0 == all flushed; >0 : still some data to be flushed; or an error code (ZSTD_isError()) */
