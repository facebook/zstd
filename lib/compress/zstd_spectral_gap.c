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

/* M_PI not guaranteed on all C standards */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Skip auto-detection for files larger than this (use default level) */
#define AUTO_MAX_FILE_SIZE ((size_t)100 * 1024 * 1024)  /* 100 MB */

static double calibrate_epsilon(const void* src, size_t srcSize) {
    double epsilon;
    size_t bound;
    void* out;
    double max_gain;
    size_t prev_size;
    int first;
    int sample_levels[9];
    int n_samples;
    int i;

    epsilon = M_PI / sqrt((double)SPECTRAL_GAP_MAX_LEVEL);
    if (epsilon > 0.02) epsilon = 0.02;
    if (epsilon < 0.0005) epsilon = 0.0005;

    bound = ZSTD_compressBound(srcSize);
    out = malloc(bound);
    if (!out) return epsilon;

    max_gain = 0.0;
    prev_size = 0;
    first = 1;
    sample_levels[0] = 1; sample_levels[1] = 3; sample_levels[2] = 5;
    sample_levels[3] = 7; sample_levels[4] = 9; sample_levels[5] = 11;
    sample_levels[6] = 15; sample_levels[7] = 19; sample_levels[8] = 22;
    n_samples = 9;

    for (i = 0; i < n_samples; i++) {
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
    double epsilon;
    size_t bound;
    void* out;
    size_t prev_size;
    int optimal_level;
    int converged_streak;
    int first;
    int level;

    if (!src || srcSize == 0) return 1;
    /* For very large files, auto-detection would be too expensive.
     * Fall back to default level — the spectral gap still applies
     * at the block level inside ZSTD_compress. */
    if (srcSize > AUTO_MAX_FILE_SIZE) return 3;

    epsilon = calibrate_epsilon(src, srcSize);
    if (out_epsilon) *out_epsilon = epsilon;

    bound = ZSTD_compressBound(srcSize);
    out = malloc(bound);
    if (!out) return 3;

    prev_size = 0;
    optimal_level = 1;
    converged_streak = 0;
    first = 1;

    for (level = 1; level <= SPECTRAL_GAP_MAX_LEVEL; level++) {
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
