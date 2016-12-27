
#include <stddef.h>   /* size_t */

typedef struct ZSTDMT_CCtx_s ZSTDMT_CCtx;

ZSTDMT_CCtx *ZSTDMT_createCCtx(unsigned nbThreads);
size_t ZSTDMT_freeCCtx(ZSTDMT_CCtx* cctx);

size_t ZSTDMT_compressCCtx(ZSTDMT_CCtx* cctx,
                           void* dst, size_t dstCapacity,
                     const void* src, size_t srcSize,
                           int compressionLevel);
