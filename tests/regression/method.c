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
 *     buffer_state_t* state = container_of(base, buffer_state_t, base);
 */
#define container_of(ptr, type, member) \
    ((type*)(ptr == NULL ? NULL : (char*)(ptr)-offsetof(type, member)))

/** State to reuse the same buffers between compression calls. */
typedef struct {
    method_state_t base;
    data_buffers_t inputs; /**< The input buffer for each file. */
    data_buffer_t compressed; /**< The compressed data buffer. */
    data_buffer_t decompressed; /**< The decompressed data buffer. */
} buffer_state_t;

static size_t buffers_max_size(data_buffers_t buffers) {
    size_t max = 0;
    for (size_t i = 0; i < buffers.size; ++i) {
        if (buffers.buffers[i].size > max)
            max = buffers.buffers[i].size;
    }
    return max;
}

static method_state_t* buffer_state_create(data_t const* data) {
    buffer_state_t* state = (buffer_state_t*)calloc(1, sizeof(buffer_state_t));
    if (state == NULL)
        return NULL;
    state->base.data = data;
    state->inputs = data_buffers_get(data);
    size_t const max_size = buffers_max_size(state->inputs);
    state->compressed = data_buffer_create(ZSTD_compressBound(max_size));
    state->decompressed = data_buffer_create(max_size);
    return &state->base;
}

static void buffer_state_destroy(method_state_t* base) {
    if (base == NULL)
        return;
    buffer_state_t* state = container_of(base, buffer_state_t, base);
    free(state);
}

static int buffer_state_bad(buffer_state_t const* state) {
    if (state == NULL) {
        fprintf(stderr, "buffer_state_t is NULL\n");
        return 1;
    }
    if (state->inputs.size == 0 || state->compressed.data == NULL ||
        state->decompressed.data == NULL) {
        fprintf(stderr, "buffer state allocation failure\n");
        return 1;
    }
    return 0;
}

static result_t simple_compress(method_state_t* base, config_t const* config) {
    buffer_state_t* state = container_of(base, buffer_state_t, base);

    if (buffer_state_bad(state))
        return result_error(result_error_system_error);

    /* Keep the tests short by skipping directories, since behavior shouldn't
     * change.
     */
    if (base->data->type != data_type_file)
        return result_error(result_error_skip);

    if (config->use_dictionary || config->no_pledged_src_size)
        return result_error(result_error_skip);

    /* If the config doesn't specify a level, skip. */
    int const level = config_get_level(config);
    if (level == CONFIG_NO_LEVEL)
        return result_error(result_error_skip);

    data_buffer_t const input = state->inputs.buffers[0];

    /* Compress, decompress, and check the result. */
    state->compressed.size = ZSTD_compress(
        state->compressed.data,
        state->compressed.capacity,
        input.data,
        input.size,
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
    if (data_buffer_compare(input, state->decompressed))
        return result_error(result_error_round_trip_error);

    result_data_t data;
    data.total_size = state->compressed.size;
    return result_data(data);
}

static result_t compress_cctx_compress(
    method_state_t* base,
    config_t const* config) {
    buffer_state_t* state = container_of(base, buffer_state_t, base);

    if (buffer_state_bad(state))
        return result_error(result_error_system_error);

    if (config->use_dictionary || config->no_pledged_src_size)
        return result_error(result_error_skip);

    if (base->data->type != data_type_dir)
        return result_error(result_error_skip);

    int const level = config_get_level(config);
    if (level == CONFIG_NO_LEVEL)
        return result_error(result_error_skip);

    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    if (cctx == NULL) {
        fprintf(stderr, "ZSTD_createCCtx() failed\n");
        return result_error(result_error_system_error);
    }

    result_t result;
    result_data_t data = {.total_size = 0};
    for (size_t i = 0; i < state->inputs.size; ++i) {
        data_buffer_t const input = state->inputs.buffers[i];

        state->compressed.size = ZSTD_compressCCtx(
            cctx,
            state->compressed.data,
            state->compressed.capacity,
            input.data,
            input.size,
            level);
        if (ZSTD_isError(state->compressed.size)) {
            result = result_error(result_error_compression_error);
            goto out;
        }

        state->decompressed.size = ZSTD_decompress(
            state->decompressed.data,
            state->decompressed.capacity,
            state->compressed.data,
            state->compressed.size);
        if (ZSTD_isError(state->decompressed.size)) {
            result = result_error(result_error_decompression_error);
            goto out;
        }
        if (data_buffer_compare(input, state->decompressed)) {
            result = result_error(result_error_round_trip_error);
            goto out;
        }

        data.total_size += state->compressed.size;
    }

    result = result_data(data);
out:
    ZSTD_freeCCtx(cctx);
    return result;
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

static result_t cli_compress(
    method_state_t* state,
    config_t const* config) {
    if (config->cli_args == NULL)
        return result_error(result_error_skip);

    /* We don't support no pledged source size with directories. Too slow. */
    if (state->data->type == data_type_dir && config->no_pledged_src_size)
        return result_error(result_error_skip);

    if (g_zstdcli == NULL)
        return result_error(result_error_system_error);

    /* '<zstd>' -cqr <args> [-D '<dict>'] '<file/dir>' */
    char cmd[1024];
    size_t const cmd_size = snprintf(
        cmd,
        sizeof(cmd),
        "'%s' -cqr %s %s%s%s %s '%s'",
        g_zstdcli,
        config->cli_args,
        config->use_dictionary ? "-D '" : "",
        config->use_dictionary ? state->data->dict.path : "",
        config->use_dictionary ? "'" : "",
        config->no_pledged_src_size ? "<" : "",
        state->data->data.path);
    if (cmd_size >= sizeof(cmd)) {
        fprintf(stderr, "command too large: %s\n", cmd);
        return result_error(result_error_system_error);
    }
    FILE* zstd = popen(cmd, "r");
    if (zstd == NULL) {
        fprintf(stderr, "failed to popen command: %s\n", cmd);
        return result_error(result_error_system_error);
    }

    char out[4096];
    size_t total_size = 0;
    while (1) {
        size_t const size = fread(out, 1, sizeof(out), zstd);
        total_size += size;
        if (size != sizeof(out))
            break;
    }
    if (ferror(zstd) || pclose(zstd) != 0) {
        fprintf(stderr, "zstd failed with command: %s\n", cmd);
        return result_error(result_error_compression_error);
    }

    result_data_t const data = {.total_size = total_size};
    return result_data(data);
}

method_t const simple = {
    .name = "ZSTD_compress",
    .create = buffer_state_create,
    .compress = simple_compress,
    .destroy = buffer_state_destroy,
};

method_t const compress_cctx = {
    .name = "ZSTD_compressCCtx",
    .create = buffer_state_create,
    .compress = compress_cctx_compress,
    .destroy = buffer_state_destroy,
};

method_t const cli = {
    .name = "zstdcli",
    .create = method_state_create,
    .compress = cli_compress,
    .destroy = method_state_destroy,
};

static method_t const* g_methods[] = {
    &simple,
    &compress_cctx,
    &cli,
    NULL,
};

method_t const* const* methods = g_methods;
