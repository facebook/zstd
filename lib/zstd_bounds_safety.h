/*
 * zstd_bounds_safety.h - portability macros for optional -fbounds-safety
 *
 * Copyright (c) 2026 Jeff Bindel <jeff@incrediblybased.co>
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 *
 * When ZSTD_SUPPORT_FBOUNDS_SAFETY is defined (typically via
 * -DZSTD_SUPPORT_FBOUNDS_SAFETY and a Clang toolchain that implements
 * -fbounds-safety), these macros expand to Clang bounds annotations.
 * Otherwise they expand to nothing so default builds are unchanged.
 *
 * Pattern matches libwebp / libpng / giflib / lz4 inert-macro
 * -fbounds-safety adoption: annotations are inert unless explicitly enabled.
 */
#ifndef ZSTD_BOUNDS_SAFETY_H_235446
#define ZSTD_BOUNDS_SAFETY_H_235446

#ifdef ZSTD_SUPPORT_FBOUNDS_SAFETY

#  include <ptrcheck.h>
/* Non-ABI-breaking sized-by annotations for byte buffers whose companion
 * field / argument is a capacity in bytes (ZSTD_inBuffer.size /
 * ZSTD_outBuffer.size). Use *_OR_NULL when the pointer may be NULL while
 * the companion size is zero.
 */
#  define ZSTD_SIZED_BY(n) __sized_by(n)
#  define ZSTD_SIZED_BY_OR_NULL(n) __sized_by_or_null(n)
#  define ZSTD_COUNTED_BY(n) __counted_by(n)
#  define ZSTD_COUNTED_BY_OR_NULL(n) __counted_by_or_null(n)

#else /* !ZSTD_SUPPORT_FBOUNDS_SAFETY */

#  define ZSTD_SIZED_BY(n)
#  define ZSTD_SIZED_BY_OR_NULL(n)
#  define ZSTD_COUNTED_BY(n)
#  define ZSTD_COUNTED_BY_OR_NULL(n)

#endif /* ZSTD_SUPPORT_FBOUNDS_SAFETY */

#endif /* ZSTD_BOUNDS_SAFETY_H_235446 */
