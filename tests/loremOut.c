/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* Implementation notes:
 * Generates a stream of Lorem ipsum paragraphs to stdout,
 * up to the requested size, which can be very large (> 4 GB).
 * Note that, beyond 1 paragraph, this generator produces
 * a different content than LOREM_genBuffer (even when using same seed).
 */

#include "loremOut.h"
#include <assert.h>
#include <stdio.h>
#include "lorem.h"    /* LOREM_genBlock */
#include "platform.h" /* Compiler options, SET_BINARY_MODE */

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LOREM_BLOCKSIZE (1 << 10)
void LOREM_genOut(unsigned long long size, unsigned seed)
{
    char buff[LOREM_BLOCKSIZE] = { 0 };
    unsigned long long total   = 0;
    size_t genBlockSize        = (size_t)MIN(size, LOREM_BLOCKSIZE);

    /* init */
    SET_BINARY_MODE(stdout);

    /* Generate Ipsum text, one paragraph at a time */
    while (total < size) {
        size_t generated =
                LOREM_genBlock(buff, genBlockSize, seed++, total == 0, 0);
        assert(generated <= genBlockSize);
        total += generated;
        assert(total <= size);
        fwrite(buff,
               1,
               generated,
               stdout); /* note: should check potential write error */
        if (size - total < genBlockSize)
            genBlockSize = (size_t)(size - total);
    }
    assert(total == size);
}
