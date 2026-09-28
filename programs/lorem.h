/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* lorem ipsum generator */

#include <stddef.h>   /* size_t */

/*
 * LOREM_genBuffer():
 * Generate @size bytes of compressible data using lorem ipsum generator
 * into provided @buffer.
 */
void LOREM_genBuffer(void* buffer, size_t size, unsigned seed);

/*
 * LOREM_genBlock():
 * Similar to LOREM_genBuffer, with additional controls :
 * - @first : generate the first sentence
 * - @fill : fill the entire @buffer,
 *           if ==0: generate one paragraph at most.
 * @return : nb of bytes generated into @buffer.
 */
size_t LOREM_genBlock(void* buffer, size_t size,
                      unsigned seed,
                      int first, int fill);
