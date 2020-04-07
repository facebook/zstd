/**
 * \file zstd.c
 * Single-file Zstandard library.
 *
 * Generate using:
 * \code
 *	combine.sh -r ../../lib -r ../../lib/common -r ../../lib/compress -r ../../lib/decompress -k zstd.h -o zstd.c zstd-in.c
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
 * Settings to bake for the single library file.
 *
 * Note: It's important that none of these affects 'zstd.h' (only the
 * implementation files we're amalgamating).
 *
 * Note: MEM_MODULE stops xxhash redefining BYTE, U16, etc., which are also
 * defined in mem.h (breaking C99 compatibility).
 *
 * Note: the undefs for xxHash allow Zstd's implementation to coinside with with
 * standalone xxHash usage (with global defines).
 *
 * Note: multithreading is enabled for all platforms apart from Emscripten.
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
#define ZSTD_LIB_DICTBUILDER 0
#define ZSTD_LIB_DEPRECATED 0
#define ZSTD_NOBENCH
#ifndef __EMSCRIPTEN__
#define ZSTD_MULTITHREAD
#endif

/* lib/common */
#include "debug.c"
#include "entropy_common.c"
#include "error_private.c"
#include "fse_decompress.c"
#include "threading.c"
#include "pool.c"
#include "zstd_common.c"

/* lib/compress */
#include "fse_compress.c"
#include "hist.c"
#include "huf_compress.c"
#include "zstd_compress_literals.c"
#include "zstd_compress_sequences.c"
#include "zstd_compress_superblock.c"
#include "zstd_compress.c"
#include "zstd_double_fast.c"
#include "zstd_fast.c"
#include "zstd_lazy.c"
#include "zstd_ldm.c"
#include "zstd_opt.c"
#ifdef ZSTD_MULTITHREAD
#include "zstdmt_compress.c"
#endif

/* lib/decompress */
#include "huf_decompress.c"
#include "zstd_ddict.c"
#include "zstd_decompress.c"
#include "zstd_decompress_block.c"
