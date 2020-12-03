/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * This file includes every .c file needed for decompression.
 * It is used by lib/decompress_unzstd.c to include the decompression
 * source into the translation-unit, so it can be used for kernel
 * decompression.
 */

#include "common/debug.c"
#include "common/entropy_common.c"
#include "common/error_private.c"
#include "common/fse_decompress.c"
#include "common/zstd_common.c"
#include "decompress/huf_decompress.c"
#include "decompress/zstd_ddict.c"
#include "decompress/zstd_decompress.c"
#include "decompress/zstd_decompress_block.c"
#include "zstd_decompress_module.c"
