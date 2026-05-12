/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/*
 * Shared annotation infrastructure for zstd's public headers.
 *
 * Defines feature-check polyfills and the macros that decorate the public
 * declarations with information used by Swift, clang's static analyser, and
 * other tooling:
 *
 *   ZSTD_NULLABILITY      Switch (1/0).  Default on for clang.  Including
 *                         code can `#define ZSTD_NULLABILITY 0` BEFORE the
 *                         first inclusion of a zstd header to suppress all
 *                         the nullability output below — this is what the
 *                         legacy `libzstd` Swift umbrella does to preserve
 *                         its pre-annotation surface.
 *
 *   ZSTD_NULLABLE         Expands to `_Nullable` when ZSTD_NULLABILITY is
 *                         on, empty otherwise.  Used in declarations to mark
 *                         the specific pointers that genuinely accept NULL.
 *                         The headers' API regions are wrapped in
 *                         `#pragma clang assume_nonnull` so pointers
 *                         without this marker default to non-null.
 *
 *   ZSTD_ENUM_OPEN
 *   ZSTD_ENUM_CLOSED      `__attribute__((enum_extensibility(...)))` for
 *                         the modern Swift import — apply directly to the
 *                         `typedef enum` tag.  Gated on
 *                         ZSTD_FOR_SWIFT_MODERN_API so the legacy libzstd
 *                         module imports plain Int32 raw values.
 *
 *   ZSTD_SWIFT_FIELD(n)   `__attribute__((swift_name(n)))` for renaming a
 *                         struct field as it appears in Swift.  apinotes
 *                         can rename functions / typedefs / tags but not
 *                         fields, so this lives in the headers themselves.
 *                         Gated on ZSTD_FOR_SWIFT_MODERN_API.
 */
#ifndef ZSTD_ANNOTATIONS_H
#define ZSTD_ANNOTATIONS_H

/* `__has_feature` and `__has_attribute` are clang extensions.  The C
 * preprocessor doesn't short-circuit `defined(x) && x(...)` at parse
 * time — unknown identifiers become `0`, so e.g.
 * `__has_feature(nullability)` would become `0(nullability)` and fail
 * to parse on GCC.  The polyfills below make the macros safely expand
 * to 0 on toolchains that don't define them. */
#ifndef __has_feature
#  define __has_feature(x) 0
#endif
#ifndef __has_attribute
#  define __has_attribute(x) 0
#endif

#ifndef ZSTD_NULLABILITY
#  if __has_feature(nullability)
#    define ZSTD_NULLABILITY 1
#  else
#    define ZSTD_NULLABILITY 0
#  endif
#endif

#if ZSTD_NULLABILITY
#  define ZSTD_NULLABLE _Nullable
#else
#  define ZSTD_NULLABLE
#endif

#if defined(ZSTD_FOR_SWIFT_MODERN_API) && __has_attribute(enum_extensibility)
#  define ZSTD_ENUM_OPEN   __attribute__((enum_extensibility(open)))
#  define ZSTD_ENUM_CLOSED __attribute__((enum_extensibility(closed)))
#else
#  define ZSTD_ENUM_OPEN
#  define ZSTD_ENUM_CLOSED
#endif

#if defined(ZSTD_FOR_SWIFT_MODERN_API) && __has_attribute(swift_name)
#  define ZSTD_SWIFT_FIELD(name) __attribute__((swift_name(name)))
#else
#  define ZSTD_SWIFT_FIELD(name)
#endif

#endif /* ZSTD_ANNOTATIONS_H */
