// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/zstd.h>
#include <linux/zstd_errors.h>

// Common symbols. zstd_compress must depend on zstd_decompress.
EXPORT_SYMBOL(ZSTD_versionNumber);
EXPORT_SYMBOL(ZSTD_versionString);
EXPORT_SYMBOL(ZSTD_isError);
EXPORT_SYMBOL(ZSTD_getErrorName);
EXPORT_SYMBOL(ZSTD_getErrorCode);
EXPORT_SYMBOL(ZSTD_getErrorString);

// Decompression symbols.
EXPORT_SYMBOL(ZSTD_getFrameContentSize);
EXPORT_SYMBOL(ZSTD_getDecompressedSize);
EXPORT_SYMBOL(ZSTD_findFrameCompressedSize);
EXPORT_SYMBOL(ZSTD_freeDCtx);
EXPORT_SYMBOL(ZSTD_decompressDCtx);
EXPORT_SYMBOL(ZSTD_dParam_getBounds);
EXPORT_SYMBOL(ZSTD_DCtx_setParameter);
EXPORT_SYMBOL(ZSTD_DCtx_reset);
EXPORT_SYMBOL(ZSTD_freeDStream);
EXPORT_SYMBOL(ZSTD_initDStream);
EXPORT_SYMBOL(ZSTD_decompressStream);
EXPORT_SYMBOL(ZSTD_DStreamInSize);
EXPORT_SYMBOL(ZSTD_DStreamOutSize);
EXPORT_SYMBOL(ZSTD_decompress_usingDict);
EXPORT_SYMBOL(ZSTD_freeDDict);
EXPORT_SYMBOL(ZSTD_decompress_usingDDict);
EXPORT_SYMBOL(ZSTD_getDictID_fromDict);
EXPORT_SYMBOL(ZSTD_getDictID_fromDDict);
EXPORT_SYMBOL(ZSTD_getDictID_fromFrame);
EXPORT_SYMBOL(ZSTD_DCtx_refDDict);
EXPORT_SYMBOL(ZSTD_DCtx_refPrefix);
EXPORT_SYMBOL(ZSTD_sizeof_DCtx);
EXPORT_SYMBOL(ZSTD_sizeof_DStream);
EXPORT_SYMBOL(ZSTD_sizeof_DDict);
EXPORT_SYMBOL(ZSTD_findDecompressedSize);
EXPORT_SYMBOL(ZSTD_decompressBound);
EXPORT_SYMBOL(ZSTD_frameHeaderSize);
EXPORT_SYMBOL(ZSTD_estimateDCtxSize);
EXPORT_SYMBOL(ZSTD_estimateDStreamSize);
EXPORT_SYMBOL(ZSTD_estimateDStreamSize_fromFrame);
EXPORT_SYMBOL(ZSTD_estimateDDictSize);
EXPORT_SYMBOL(ZSTD_initStaticDCtx);
EXPORT_SYMBOL(ZSTD_initStaticDStream);
EXPORT_SYMBOL(ZSTD_initStaticDDict);
EXPORT_SYMBOL(ZSTD_createDCtx_advanced);
EXPORT_SYMBOL(ZSTD_createDStream_advanced);
EXPORT_SYMBOL(ZSTD_createDDict_advanced);
EXPORT_SYMBOL(ZSTD_isFrame);
EXPORT_SYMBOL(ZSTD_createDDict_byReference);
EXPORT_SYMBOL(ZSTD_DCtx_loadDictionary_byReference);
EXPORT_SYMBOL(ZSTD_DCtx_loadDictionary_advanced);
EXPORT_SYMBOL(ZSTD_DCtx_refPrefix_advanced);
EXPORT_SYMBOL(ZSTD_DCtx_setMaxWindowSize);
EXPORT_SYMBOL(ZSTD_DCtx_setFormat);
EXPORT_SYMBOL(ZSTD_decompressStream_simpleArgs);
EXPORT_SYMBOL(ZSTD_initDStream_usingDict);
EXPORT_SYMBOL(ZSTD_initDStream_usingDDict);
EXPORT_SYMBOL(ZSTD_resetDStream);
EXPORT_SYMBOL(ZSTD_getFrameHeader);
EXPORT_SYMBOL(ZSTD_getFrameHeader_advanced);
EXPORT_SYMBOL(ZSTD_decodingBufferSize_min);
EXPORT_SYMBOL(ZSTD_decompressBegin);
EXPORT_SYMBOL(ZSTD_decompressBegin_usingDict);
EXPORT_SYMBOL(ZSTD_decompressBegin_usingDDict);
EXPORT_SYMBOL(ZSTD_nextSrcSizeToDecompress);
EXPORT_SYMBOL(ZSTD_decompressContinue);
EXPORT_SYMBOL(ZSTD_copyDCtx);
EXPORT_SYMBOL(ZSTD_nextInputType);
EXPORT_SYMBOL(ZSTD_decompressBlock);
EXPORT_SYMBOL(ZSTD_insertBlock);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Zstd Decompressor");
