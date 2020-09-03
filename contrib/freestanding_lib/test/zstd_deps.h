/*
 * Copyright (c) 2016-2020, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* Need:
 * NULL
 * ZSTD_memcpy()
 * ZSTD_memset()
 * ZSTD_memmove()
 * BYTE
 * S16
 * U16
 * U32
 * U64
 * size_t
 * ptrdiff_t
 * INT_MAX
 * ...
 */
#ifndef ZSTD_DEPS_COMMON
#define ZSTD_DEPS_COMMON

#include <stddef.h>

typedef unsigned char      BYTE;
typedef unsigned short      U16;
typedef   signed short      S16;
typedef unsigned int        U32;
typedef   signed int        S32;
typedef unsigned long long  U64;
typedef   signed long long  S64;

#ifndef INT_MAX
# define INT_MAX ((int)((1u << 31) - 1))
#endif

#if defined(__GNUC__) && __GNUC__ >= 4
#define ZSTD_memcpy(d,s,n) __builtin_memcpy((d),(s),(n))
#define ZSTD_memmove(d,s,n) __builtin_memmove((d),(s),(n))
#define ZSTD_memset(d,s,n) __builtin_memset((d),(s),(n))
#else
void* ZSTD_memcpy(void* destination, const void* source, size_t num);
void* ZSTD_memmove(void* destination, const void* source, size_t num);
void* ZSTD_memset(void* destination, int value, size_t num);
#endif

#endif /* ZSTD_DEPS_COMMON */

/* Need:
 * ZSTD_malloc()
 * ZSTD_free()
 * ZSTD_calloc()
 */
#ifdef ZSTD_DEPS_NEED_MALLOC
#ifndef ZSTD_DEPS_MALLOC
#define ZSTD_DEPS_MALLOC

void* ZSTD_malloc(size_t size);
void ZSTD_free(void* ptr);
void* ZSTD_calloc(size_t num, size_t size);

#endif /* ZSTD_DEPS_MALLOC */
#endif /* ZSTD_DEPS_NEED_MALLOC */

/* Need:
 * assert()
 */
#ifdef ZSTD_DEPS_NEED_ASSERT
#ifndef ZSTD_DEPS_ASSERT
#define ZSTD_DEPS_ASSERT

#define assert(x) ((void)0)

#endif /* ZSTD_DEPS_ASSERT */
#endif /* ZSTD_DEPS_NEED_ASSERT */

/* Need:
 * ZSTD_DEBUG_PRINT()
 */
#ifdef ZSTD_DEPS_NEED_IO
#ifndef ZSTD_DEPS_IO
#define ZSTD_DEPS_IO

#define ZSTD_DEBUG_PRINT(...) 

#endif /* ZSTD_DEPS_IO */
#endif /* ZSTD_DEPS_NEED_IO */

/* Only requested when <stdint.h> is known to be present.
 * Need:
 * intptr_t
 */
#ifdef ZSTD_DEPS_NEED_STDINT
#ifndef ZSTD_DEPS_STDINT
#define ZSTD_DEPS_STDINT

#define intptr_t size_t

#endif /* ZSTD_DEPS_STDINT */
#endif /* ZSTD_DEPS_NEED_STDINT */
