// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/zstd.h>

#include "zstd.h"
#include "common/zstd_deps.h"
#include "common/zstd_errors.h"

/* Common symbols. zstd_compress must depend on zstd_decompress. */

unsigned int zstd_is_error(size_t code)
{
	return ZSTD_isError(code);
}
EXPORT_SYMBOL(zstd_is_error);

int zstd_get_error_code(size_t code)
{
	return ZSTD_getErrorCode(code);
}
EXPORT_SYMBOL(zstd_get_error_code);

const char *zstd_get_error_name(size_t code)
{
	return ZSTD_getErrorName(code);
}
EXPORT_SYMBOL(zstd_get_error_name);

/* Decompression symbols. */

size_t zstd_dctx_workspace_bound(void)
{
	return ZSTD_estimateDCtxSize();
}
EXPORT_SYMBOL(zstd_dctx_workspace_bound);

zstd_dctx *zstd_init_dctx(void *workspace, size_t workspace_size)
{
	if (workspace == NULL)
		return NULL;
	return ZSTD_initStaticDCtx(workspace, workspace_size);
}
EXPORT_SYMBOL(zstd_init_dctx);

size_t zstd_decompress_dctx(zstd_dctx *dctx, void *dst, size_t dst_capacity,
	const void *src, size_t src_size)
{
	return ZSTD_decompressDCtx(dctx, dst, dst_capacity, src, src_size);
}
EXPORT_SYMBOL(zstd_decompress_dctx);

size_t zstd_dstream_workspace_bound(size_t max_window_size)
{
	return ZSTD_estimateDStreamSize(max_window_size);
}
EXPORT_SYMBOL(zstd_dstream_workspace_bound);

zstd_dstream *zstd_init_dstream(size_t max_window_size, void *workspace,
	size_t workspace_size)
{
	if (workspace == NULL)
		return NULL;
	(void)max_window_size;
	return ZSTD_initStaticDStream(workspace, workspace_size);
}
EXPORT_SYMBOL(zstd_init_dstream);

size_t zstd_reset_dstream(zstd_dstream *dstream)
{
	return ZSTD_resetDStream(dstream);
}
EXPORT_SYMBOL(zstd_reset_dstream);

size_t zstd_decompress_stream(zstd_dstream *dstream,
	struct zstd_out_buffer *output, struct zstd_in_buffer *input)
{
	ZSTD_outBuffer o;
	ZSTD_inBuffer i;
	size_t ret;

	ZSTD_memcpy(&o, output, sizeof(o));
	ZSTD_memcpy(&i, input, sizeof(i));
	ret = ZSTD_decompressStream(dstream, &o, &i);
	ZSTD_memcpy(output, &o, sizeof(o));
	ZSTD_memcpy(input, &i, sizeof(i));
	return ret;
}
EXPORT_SYMBOL(zstd_decompress_stream);

size_t zstd_find_frame_compressed_size(const void *src, size_t src_size)
{
	return ZSTD_findFrameCompressedSize(src, src_size);
}
EXPORT_SYMBOL(zstd_find_frame_compressed_size);

size_t zstd_get_frame_params(struct zstd_frame_params *params, const void *src,
	size_t src_size)
{
	ZSTD_frameHeader h;
	const size_t ret = ZSTD_getFrameHeader(&h, src, src_size);

	if (ret != 0)
		return ret;

	if (h.frameContentSize != ZSTD_CONTENTSIZE_UNKNOWN)
		params->frame_content_size = h.frameContentSize;
	else
		params->frame_content_size = 0;

	params->window_size = h.windowSize;
	params->dict_id = h.dictID;
	params->checksum_flag = h.checksumFlag;

	return ret;
}
EXPORT_SYMBOL(zstd_get_frame_params);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Zstd Decompressor");
