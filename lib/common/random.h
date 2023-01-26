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
 * - `ZSTD_random_state_t`a struct that holds the secure random state. Should be
 *   initialized with zeroes before first usage.
 * - `size_t getSecureRandom(ZSTD_random_state_t *state, void *buf, size_t buflen)`
 *   a function that exists if `HAS_SECURE_RANDOM` is defined. Fills buffer with
 *   securely generated random bytes, returns 0 on success.
 */

#if defined(__GLIBC__) && \
  (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))

#define HAS_SECURE_RANDOM

#include <sys/random.h>

typedef struct {
    U8 numbytes;
    U8 bytes[255];
} ZSTD_random_state_t;

MEM_STATIC size_t getSecureRandom(ZSTD_random_state_t *state, void *buf, size_t buflen) {
    U8 *bytesBuf = (U8 *) buf;
    while (buflen) {
        if (state->numbytes == 0) {
            int randomBytes = getrandom(state->bytes, 255, GRND_NONBLOCK);
            if (randomBytes <= 0) {
                return -1;
            }
            state->numbytes = (U8)randomBytes;
        }
        {
            size_t n = buflen < (size_t)state->numbytes ? buflen : (size_t)state->numbytes;
            ZSTD_memcpy(bytesBuf, state->bytes, n);
            ZSTD_memmove(state->bytes, state->bytes + n, state->numbytes - n);
            state->numbytes -= n;
            buflen -= n;
        }
    }
    return 0;
}

#endif

#endif /* ZSTD_RANDOM_H */
