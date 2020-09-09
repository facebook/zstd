// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/zstd.h>

EXPORT_SYMBOL(ZSTD_compressBound);
EXPORT_SYMBOL(ZSTD_minCLevel);
EXPORT_SYMBOL(ZSTD_maxCLevel);
EXPORT_SYMBOL(ZSTD_freeCCtx);
EXPORT_SYMBOL(ZSTD_compressCCtx);
EXPORT_SYMBOL(ZSTD_cParam_getBounds);
EXPORT_SYMBOL(ZSTD_CCtx_setParameter);
EXPORT_SYMBOL(ZSTD_CCtx_setPledgedSrcSize);
EXPORT_SYMBOL(ZSTD_CCtx_reset);
EXPORT_SYMBOL(ZSTD_compress2);
EXPORT_SYMBOL(ZSTD_freeCStream);
EXPORT_SYMBOL(ZSTD_compressStream2);
EXPORT_SYMBOL(ZSTD_CStreamInSize);
EXPORT_SYMBOL(ZSTD_CStreamOutSize);
EXPORT_SYMBOL(ZSTD_initCStream);
EXPORT_SYMBOL(ZSTD_compressStream);
EXPORT_SYMBOL(ZSTD_flushStream);
EXPORT_SYMBOL(ZSTD_endStream);
EXPORT_SYMBOL(ZSTD_compress_usingDict);
EXPORT_SYMBOL(ZSTD_freeCDict);
EXPORT_SYMBOL(ZSTD_compress_usingCDict);
EXPORT_SYMBOL(ZSTD_CCtx_refCDict);
EXPORT_SYMBOL(ZSTD_CCtx_refPrefix);
EXPORT_SYMBOL(ZSTD_sizeof_CCtx);
EXPORT_SYMBOL(ZSTD_sizeof_CStream);
EXPORT_SYMBOL(ZSTD_sizeof_CDict);
EXPORT_SYMBOL(ZSTD_getSequences);
EXPORT_SYMBOL(ZSTD_estimateCCtxSize);
EXPORT_SYMBOL(ZSTD_estimateCCtxSize_usingCParams);
EXPORT_SYMBOL(ZSTD_estimateCCtxSize_usingCCtxParams);
EXPORT_SYMBOL(ZSTD_estimateCStreamSize);
EXPORT_SYMBOL(ZSTD_estimateCStreamSize_usingCParams);
EXPORT_SYMBOL(ZSTD_estimateCDictSize);
EXPORT_SYMBOL(ZSTD_estimateCDictSize_advanced);
EXPORT_SYMBOL(ZSTD_initStaticCCtx);
EXPORT_SYMBOL(ZSTD_initStaticCStream);
EXPORT_SYMBOL(ZSTD_initStaticCDict);
EXPORT_SYMBOL(ZSTD_createCCtx_advanced);
EXPORT_SYMBOL(ZSTD_createCStream_advanced);
EXPORT_SYMBOL(ZSTD_createCDict_advanced);
EXPORT_SYMBOL(ZSTD_createCDict_byReference);
EXPORT_SYMBOL(ZSTD_getCParams);
EXPORT_SYMBOL(ZSTD_getParams);
EXPORT_SYMBOL(ZSTD_checkCParams);
EXPORT_SYMBOL(ZSTD_adjustCParams);
EXPORT_SYMBOL(ZSTD_compress_advanced);
EXPORT_SYMBOL(ZSTD_compress_usingCDict_advanced);
EXPORT_SYMBOL(ZSTD_CCtx_loadDictionary_byReference);
EXPORT_SYMBOL(ZSTD_CCtx_loadDictionary_advanced);
EXPORT_SYMBOL(ZSTD_CCtx_refPrefix_advanced);
EXPORT_SYMBOL(ZSTD_CCtx_getParameter);
EXPORT_SYMBOL(ZSTD_compressStream2_simpleArgs);
EXPORT_SYMBOL(ZSTD_initCStream_srcSize);
EXPORT_SYMBOL(ZSTD_initCStream_usingDict);
EXPORT_SYMBOL(ZSTD_initCStream_advanced);
EXPORT_SYMBOL(ZSTD_initCStream_usingCDict);
EXPORT_SYMBOL(ZSTD_initCStream_usingCDict_advanced);
EXPORT_SYMBOL(ZSTD_resetCStream);
EXPORT_SYMBOL(ZSTD_getFrameProgression);
EXPORT_SYMBOL(ZSTD_toFlushNow);
EXPORT_SYMBOL(ZSTD_compressBegin);
EXPORT_SYMBOL(ZSTD_compressBegin_usingDict);
EXPORT_SYMBOL(ZSTD_compressBegin_advanced);
EXPORT_SYMBOL(ZSTD_compressBegin_usingCDict);
EXPORT_SYMBOL(ZSTD_compressBegin_usingCDict_advanced);
EXPORT_SYMBOL(ZSTD_copyCCtx);
EXPORT_SYMBOL(ZSTD_compressContinue);
EXPORT_SYMBOL(ZSTD_compressEnd);
EXPORT_SYMBOL(ZSTD_getBlockSize);
EXPORT_SYMBOL(ZSTD_compressBlock);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Zstd Compressor");
