/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "method.h"

#include <stdio.h>
#include <stdlib.h>

#include <zstd.h>

static char const* g_zstdcli = NULL;

void method_set_zstdcli(char const* zstdcli) {
    g_zstdcli = zstdcli;
}

/**
 * Macro to get a pointer of type, given ptr, which is a member variable with
 * the given name, member.
 *
 *     method_state_t* base = ...;
 *     simple_state_t* state = container_of(base, simple_state_t, base);
 */
#define container_of(ptr, type, member) \
    ((type*)(char*)(ptr)-offsetof(type, member))

/** State to reuse the same buffers between compression calls. */
typedef struct {
    method_state_t base;
    data_buffer_t buffer;        /**< The constant input data buffer. */
    data_buffer_t compressed;    /**< The compressed data buffer. */
    data_buffer_t decompressed;  /**< The decompressed data buffer. */
} simple_state_t;

static method_state_t* simple_create(data_t const* data) {
    simple_state_t* state = (simple_state_t*)calloc(1, sizeof(simple_state_t));
    if (state == NULL)
        return NULL;
    state->base.data = data;
    state->buffer = data_buffer_get(data);
    state->compressed =
        data_buffer_create(ZSTD_compressBound(state->buffer.size));
    state->decompressed = data_buffer_create(state->buffer.size);
    return &state->base;
}

static void simple_destroy(method_state_t* base) {
    if (base == NULL)
        return;
    simple_state_t* state = container_of(base, simple_state_t, base);
    free(state);
}

static result_t simple_compress(method_state_t* base, config_t const* config) {
    if (base == NULL)
        return result_error(result_error_system_error);
    simple_state_t* state = container_of(base, simple_state_t, base);

    if (base->data->type != data_type_file)
        return result_error(result_error_skip);

    if (state->buffer.data == NULL || state->compressed.data == NULL ||
        state->decompressed.data == NULL) {
        return result_error(result_error_system_error);
    }

    /* If the config doesn't specify a level, skip. */
    int const level = config_get_level(config);
    if (level == CONFIG_NO_LEVEL)
        return result_error(result_error_skip);

    /* Compress, decompress, and check the result. */
    state->compressed.size = ZSTD_compress(
        state->compressed.data,
        state->compressed.capacity,
        state->buffer.data,
        state->buffer.size,
        level);
    if (ZSTD_isError(state->compressed.size))
        return result_error(result_error_compression_error);

    state->decompressed.size = ZSTD_decompress(
        state->decompressed.data,
        state->decompressed.capacity,
        state->compressed.data,
        state->compressed.size);
    if (ZSTD_isError(state->decompressed.size))
        return result_error(result_error_decompression_error);
    if (data_buffer_compare(state->buffer, state->decompressed))
        return result_error(result_error_round_trip_error);

    result_data_t data;
    data.total_size = state->compressed.size;
    return result_data(data);
}

/** Generic state creation function. */
static method_state_t* method_state_create(data_t const* data) {
    method_state_t* state = (method_state_t*)malloc(sizeof(method_state_t));
    if (state == NULL)
        return NULL;
    state->data = data;
    return state;
}

static void method_state_destroy(method_state_t* state) {
    free(state);
}

#define MAX_OUT 32

static result_t cli_file_compress(
    method_state_t* state,
    config_t const* config) {
    if (config->cli_args == NULL)
        return result_error(result_error_skip);

    if (g_zstdcli == NULL)
        return result_error(result_error_system_error);

    /* '<zstd>' -r <args> '<file/dir>' | wc -c */
    char cmd[1024];
    size_t const cmd_size = snprintf(
        cmd,
        sizeof(cmd),
        "'%s' -cqr %s '%s' | wc -c",
        g_zstdcli,
        config->cli_args,
        state->data->path);
    if (cmd_size >= sizeof(cmd)) {
        fprintf(stderr, "command too large: %s\n", cmd);
        return result_error(result_error_system_error);
    }
    FILE* zstd = popen(cmd, "r");
    if (zstd == NULL) {
        fprintf(stderr, "failed to popen command: %s\n", cmd);
        return result_error(result_error_system_error);
    }

    /* Read the total compressed size. */
    char out[MAX_OUT + 1];
    size_t const out_size = fread(out, 1, MAX_OUT, zstd);
    out[out_size] = '\0';
    int const zstd_ret = pclose(zstd);
    if (zstd_ret != 0) {
        fprintf(stderr, "zstd failed with command: %s\n", cmd);
        return result_error(result_error_compression_error);
    }
    if (out_size == MAX_OUT) {
        fprintf(stderr, "wc -c produced more bytes than expected: %s\n", out);
        return result_error(result_error_system_error);
    }

    result_data_t data;
    data.total_size = atoll(out);
    return result_data(data);
}

method_t const simple = {
    .name = "simple",
    .create = simple_create,
    .compress = simple_compress,
    .destroy = simple_destroy,
};

method_t const cli_file = {
    .name = "cli file",
    .create = method_state_create,
    .compress = cli_file_compress,
    .destroy = method_state_destroy,
};

static method_t const* g_methods[] = {
    &simple,
    &cli_file,
    NULL,
};

method_t const* const* methods = g_methods;
