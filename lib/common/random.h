/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_RANDOM_H
#define ZSTD_RANDOM_H

#include "mem.h"

/*
 * Portability helpers for secure random, exposes the following:
 * - `HAS_SECURE_RANDOM` a macro that determines if secure random API is available on
 *   the platform.
 * - `size_t getSecureRandom(void *buf, size_t buflen)` - exists if `HAS_SECURE_RANDOM` is
 *   defined. Fills buffer with securely generated random bytes, returns 0 on success.
 */

#if defined(__GLIBC__) && \
  (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))

#define HAS_SECURE_RANDOM
#include <sys/random.h>
MEM_STATIC size_t getSecureRandom(void *buf, size_t buflen) {
    return getrandom(buf, buflen, GRND_NONBLOCK) != (ssize_t) buflen;
}


#endif


#endif /* ZSTD_RANDOM_H */
