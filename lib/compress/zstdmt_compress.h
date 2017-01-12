
/* ===   Dependencies   === */
#include <stddef.h>   /* size_t */
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
size_t ZSTDMT_compressStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output, ZSTD_inBuffer* input);
size_t ZSTDMT_flushStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output);
size_t ZSTDMT_endStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output);
