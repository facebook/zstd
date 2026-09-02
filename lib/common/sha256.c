/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "sha256.h"

#include <string.h>

#include "bits.h"
#include "mem.h"
#include "zstd_deps.h"

#define ZSTD_SHA256_BLOCK_SIZE 64

#define ZSTD_SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define ZSTD_SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define ZSTD_SHA256_SIGMA0(x) \
    (ZSTD_rotateRight_U32(x,  2) ^ \
     ZSTD_rotateRight_U32(x, 13) ^ \
     ZSTD_rotateRight_U32(x, 22))
#define ZSTD_SHA256_SIGMA1(x) \
    (ZSTD_rotateRight_U32(x,  6) ^ \
     ZSTD_rotateRight_U32(x, 11) ^ \
     ZSTD_rotateRight_U32(x, 25))
#define ZSTD_SHA256_sigma0(x) \
    (ZSTD_rotateRight_U32(x,  7) ^ \
     ZSTD_rotateRight_U32(x, 18) ^ \
     ((x) >>  3))
#define ZSTD_SHA256_sigma1(x) \
    (ZSTD_rotateRight_U32(x, 17) ^ \
     ZSTD_rotateRight_U32(x, 19) ^ \
     ((x) >> 10))


static const uint32_t I[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void ZSTD_SHA256_block(uint32_t hash[8], const uint8_t block[64]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h, t;
    for (t = 0; t < 16; t++) {
        w[t] = MEM_readBE32(block);
        block += 4;
    }
    for (; t < 64; t++) {
        w[t] = ZSTD_SHA256_sigma1(w[t - 2]) + w[t - 7]
             + ZSTD_SHA256_sigma0(w[t - 15]) + w[t - 16];
    }
    a = hash[0];
    b = hash[1];
    c = hash[2];
    d = hash[3];
    e = hash[4];
    f = hash[5];
    g = hash[6];
    h = hash[7];
    for (t = 0; t < 64; t++) {
        const uint32_t t1 = h + ZSTD_SHA256_SIGMA1(e) + ZSTD_SHA256_CH(e, f, g)
                          + K[t] + w[t];
        const uint32_t t2 = ZSTD_SHA256_SIGMA0(a) + ZSTD_SHA256_MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
    hash[5] += f;
    hash[6] += g;
    hash[7] += h;
}

static void ZSTD_SHA256_finish(
        uint32_t hash[8],
        const void* msg_tail,
        const size_t remaining,
        const size_t total_size) {
    uint8_t buf[ZSTD_SHA256_BLOCK_SIZE];
    uint64_t total_bits = total_size * 8;
    uint32_t i;
    assert(remaining < 64);
    ZSTD_memcpy(buf, msg_tail, remaining);
    buf[remaining] = '\x80';
    ZSTD_memset(buf + remaining + 1, 0, ZSTD_SHA256_BLOCK_SIZE - remaining - 1);
    if (remaining > 55) {
        ZSTD_SHA256_block(hash, buf);
        ZSTD_memset(buf, 0, ZSTD_SHA256_BLOCK_SIZE);
    }
    for (i = 0; i < 8; i++) {
        buf[56 + i] = (uint8_t)(total_bits >> ((7 - i) * 8));
    }
    ZSTD_SHA256_block(hash, buf);
}

static ZSTD_SHA256_Result ZSTD_SHA256_digest(uint32_t hash[8]) {
    ZSTD_SHA256_Result result;
    uint8_t* d = result.digest;
    uint32_t i;
    for (i = 0; i < 8; i++) {
        MEM_writeBE32(d, hash[i]);
        d += 4;
    }
    return result;
}

ZSTD_SHA256_Result ZSTD_SHA256_hash(const void* const msg, const size_t size) {
    const uint8_t* cur = (const uint8_t*)msg;
    size_t remaining = size;
    uint32_t hash[8];

    /* Init hash */
    ZSTD_memcpy(hash, I, sizeof(hash));

    /* Process full blocks */
    while (remaining >= ZSTD_SHA256_BLOCK_SIZE) {
        ZSTD_SHA256_block(hash, cur);
        cur += ZSTD_SHA256_BLOCK_SIZE;
        remaining -= ZSTD_SHA256_BLOCK_SIZE;
    }

    /* Pad */
    ZSTD_SHA256_finish(hash, cur, remaining, size);

    /* Extract digest */
    return ZSTD_SHA256_digest(hash);
}
