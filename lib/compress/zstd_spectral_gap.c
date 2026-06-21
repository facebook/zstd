/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Spectral Gap Auto-Compression Level Detection.
 *
 * Method: compress incrementally at levels 1..22, compute fractional
 * gain at each step. Stop when gain < epsilon for K consecutive levels.
 * Epsilon calibrated from inter-level gains — transfers to all files.
 */

#include "zstd_spectral_gap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../zstd.h"

static double calibrate_epsilon(const void* src, size_t srcSize) {
    double epsilon = M_PI / sqrt((double)SPECTRAL_GAP_MAX_LEVEL);
    if (epsilon > 0.02) epsilon = 0.02;
    if (epsilon < 0.0005) epsilon = 0.0005;

    size_t bound = ZSTD_compressBound(srcSize);
    void* out = malloc(bound);
    if (!out) return epsilon;

    double max_gain = 0.0;
    size_t prev_size = 0;
    int first = 1;
    int sample_levels[] = {1, 3, 5, 7, 9, 11, 15, 19, 22};
    int n_samples = sizeof(sample_levels) / sizeof(sample_levels[0]);

    for (int i = 0; i < n_samples; i++) {
        size_t csize = ZSTD_compress(out, bound, src, srcSize,
                                      sample_levels[i]);
        if (ZSTD_isError(csize)) continue;

        if (first) {
            first = 0;
        } else {
            double gain = (double)((long long)prev_size - (long long)csize)
                        / (double)prev_size;
            if (gain > max_gain) {
                max_gain = gain;
                epsilon = max_gain * 0.15;
                if (epsilon > 0.02) epsilon = 0.02;
                if (epsilon < 0.0005) epsilon = 0.0005;
            }
        }
        prev_size = csize;
    }

    free(out);
    return epsilon;
}

int ZSTD_autoDetectLevel(const void* src, size_t srcSize,
                          double* out_epsilon) {
    if (!src || srcSize == 0) return 1;

    double epsilon = calibrate_epsilon(src, srcSize);
    if (out_epsilon) *out_epsilon = epsilon;

    size_t bound = ZSTD_compressBound(srcSize);
    void* out = malloc(bound);
    if (!out) return 3;

    size_t prev_size = 0;
    int optimal_level = 1;
    int converged_streak = 0;
    int first = 1;

    for (int level = 1; level <= SPECTRAL_GAP_MAX_LEVEL; level++) {
        size_t csize = ZSTD_compress(out, bound, src, srcSize, level);
        if (ZSTD_isError(csize)) break;

        if (first) {
            first = 0;
        } else {
            double gain = (double)((long long)prev_size - (long long)csize)
                        / (double)prev_size;

            if (gain >= epsilon) {
                optimal_level = level;
                converged_streak = 0;
            } else {
                converged_streak++;
                if (converged_streak >= SPECTRAL_GAP_CONVERGE_K) {
                    break;
                }
            }
        }
        prev_size = csize;
    }

    free(out);
    return optimal_level;
}
