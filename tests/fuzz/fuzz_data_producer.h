/*
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 */

/**
 * Helper APIs for generating random data from input data stream.
 The producer reads bytes from the end of the input and appends them together
 to generate  a random number in the requested range. If it runs out of input
 data, it will keep returning the same value (min) over and over again.

 */

#ifndef FUZZ_DATA_PRODUCER_H
#define FUZZ_DATA_PRODUCER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fuzz_helpers.h"

/* Struct used for maintaining the state of the data */
typedef struct FUZZ_dataProducer_s FUZZ_dataProducer_t;

/* Returns a data producer state struct. Use for producer initialization. */
FUZZ_dataProducer_t *FUZZ_dataProducer_create(const uint8_t *data, size_t size);

/* Frees the data producer */
void FUZZ_dataProducer_free(FUZZ_dataProducer_t *producer);

/* Returns value between [min, max] */
uint32_t FUZZ_dataProducer_uint32Range(FUZZ_dataProducer_t *producer, uint32_t min,
                                  uint32_t max);

/* Returns a uint32 value */
uint32_t FUZZ_dataProducer_uint32(FUZZ_dataProducer_t *producer);

/* Returns the size of the remaining bytes of data in the producer */
size_t FUZZ_dataProducer_remainingBytes(FUZZ_dataProducer_t *producer);

/* Tells the producer to contract to newSize bytes of data it currently uses,
counted from the end, and forget about the rest. If newSize > current data size,
nothing happens. Returns the number of bytes the producer won't use anymore,
after contracting. */
size_t FUZZ_dataProducer_contract(FUZZ_dataProducer_t *producer, size_t newSize);

#endif // FUZZ_DATA_PRODUCER_H
