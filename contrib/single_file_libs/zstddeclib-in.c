/**
 * \file zstddeclib.c
 * Single-file Zstandard decompressor.
 *
 * Generate using:
 * \code
 *	combine.sh -r ../../lib -r ../../lib/common -r ../../lib/decompress -o zstddeclib.c zstddeclib-in.c
 * \endcode
 */
/*
 * Copyright (c) 2016-2020, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */
/*
 * Settings to bake for the standalone decompressor.
 *
 * Note: It's important that none of these affects 'zstd.h' (only the
 * implementation files we're amalgamating).
 *
 * Note: MEM_MODULE stops xxhash redefining BYTE, U16, etc., which are also
 * defined in mem.h (breaking C99 compatibility).
 *
 * Note: the undefs for xxHash allow Zstd's implementation to coinside with with
 * standalone xxHash usage (with global defines).
 */
#define DEBUGLEVEL 0
#define MEM_MODULE
#undef  XXH_NAMESPACE
#define XXH_NAMESPACE ZSTD_
#undef  XXH_PRIVATE_API
#define XXH_PRIVATE_API
#undef  XXH_INLINE_ALL
#define XXH_INLINE_ALL
#define ZSTD_LEGACY_SUPPORT 0
#define ZSTD_LIB_COMPRESSION 0
#define ZSTD_LIB_DEPRECATED 0
#define ZSTD_NOBENCH
#define ZSTD_STRIP_ERROR_STRINGS

/* lib/common */
#include "debug.c"
#include "entropy_common.c"
#include "error_private.c"
#include "fse_decompress.c"
#include "zstd_common.c"

/* lib/decompress */
#include "huf_decompress.c"
#include "zstd_ddict.c"
#include "zstd_decompress.c"
#include "zstd_decompress_block.c"
