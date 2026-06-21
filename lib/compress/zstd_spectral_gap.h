/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Spectral Gap Auto-Compression Level Detection.
 * Detects optimal compression level by monitoring fractional gain convergence.
 */
#ifndef ZSTD_SPECTRAL_GAP_H
#define ZSTD_SPECTRAL_GAP_H

#include <stddef.h>

#define SPECTRAL_GAP_MAX_LEVEL     22
#define SPECTRAL_GAP_CONVERGE_K     3
#define SPECTRAL_GAP_DEFAULT_EPSILON 0.005

int ZSTD_autoDetectLevel(const void* src, size_t srcSize,
                          double* out_epsilon);

#endif /* ZSTD_SPECTRAL_GAP_H */
