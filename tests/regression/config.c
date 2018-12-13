/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "config.h"

/* Define a config for each fast level we want to test with. */
#define FAST_LEVEL(x)                                               \
    param_value_t const level_fast##x##_param_values[] = {          \
        {.param = ZSTD_c_compressionLevel, .value = -x},            \
    };                                                              \
    config_t const level_fast##x = {                                \
        .name = "level -" #x,                                       \
        .cli_args = "--fast=" #x,                                   \
        .param_values = PARAM_VALUES(level_fast##x##_param_values), \
    };                                                              \
    config_t const level_fast##x##_dict = {                         \
        .name = "level -" #x " with dict",                          \
        .cli_args = "--fast=" #x,                                   \
        .param_values = PARAM_VALUES(level_fast##x##_param_values), \
        .use_dictionary = 1,                                        \
    };

/* Define a config for each level we want to test with. */
#define LEVEL(x)                                                \
    param_value_t const level_##x##_param_values[] = {          \
        {.param = ZSTD_c_compressionLevel, .value = x},         \
    };                                                          \
    config_t const level_##x = {                                \
        .name = "level " #x,                                    \
        .cli_args = "-" #x,                                     \
        .param_values = PARAM_VALUES(level_##x##_param_values), \
    };                                                          \
    config_t const level_##x##_dict = {                         \
        .name = "level " #x " with dict",                       \
        .cli_args = "-" #x,                                     \
        .param_values = PARAM_VALUES(level_##x##_param_values), \
        .use_dictionary = 1,                                    \
    };

#define PARAM_VALUES(pv) \
    { .data = pv, .size = sizeof(pv) / sizeof((pv)[0]) }

#include "levels.h"

#undef LEVEL
#undef FAST_LEVEL

static config_t no_pledged_src_size = {
    .name = "no source size",
    .cli_args = "",
    .param_values = PARAM_VALUES(level_0_param_values),
    .no_pledged_src_size = 1,
};

static config_t const* g_configs[] = {

#define FAST_LEVEL(x) &level_fast##x, &level_fast##x##_dict,
#define LEVEL(x) &level_##x, &level_##x##_dict,
#include "levels.h"
#undef LEVEL
#undef FAST_LEVEL

    &no_pledged_src_size,
    NULL,
};

config_t const* const* configs = g_configs;

int config_skip_data(config_t const* config, data_t const* data) {
    return config->use_dictionary && !data_has_dict(data);
}

int config_get_level(config_t const* config)
{
    param_values_t const params = config->param_values;
    size_t i;
    for (i = 0; i < params.size; ++i) {
        if (params.data[i].param == ZSTD_c_compressionLevel)
            return (int)params.data[i].value;
    }
    return CONFIG_NO_LEVEL;
}

ZSTD_parameters config_get_zstd_params(
    config_t const* config,
    uint64_t srcSize,
    size_t dictSize)
{
    ZSTD_parameters zparams = {};
    param_values_t const params = config->param_values;
    int level = config_get_level(config);
    if (level == CONFIG_NO_LEVEL)
        level = 3;
    zparams = ZSTD_getParams(
        level,
        config->no_pledged_src_size ? ZSTD_CONTENTSIZE_UNKNOWN : srcSize,
        dictSize);
    for (size_t i = 0; i < params.size; ++i) {
        unsigned const value = params.data[i].value;
        switch (params.data[i].param) {
            case ZSTD_c_contentSizeFlag:
                zparams.fParams.contentSizeFlag = value;
                break;
            case ZSTD_c_checksumFlag:
                zparams.fParams.checksumFlag = value;
                break;
            case ZSTD_c_dictIDFlag:
                zparams.fParams.noDictIDFlag = !value;
                break;
            case ZSTD_c_windowLog:
                zparams.cParams.windowLog = value;
                break;
            case ZSTD_c_chainLog:
                zparams.cParams.chainLog = value;
                break;
            case ZSTD_c_hashLog:
                zparams.cParams.hashLog = value;
                break;
            case ZSTD_c_searchLog:
                zparams.cParams.searchLog = value;
                break;
            case ZSTD_c_minMatch:
                zparams.cParams.minMatch = value;
                break;
            case ZSTD_c_targetLength:
                zparams.cParams.targetLength = value;
                break;
            case ZSTD_c_strategy:
                zparams.cParams.strategy = (ZSTD_strategy)value;
                break;
            default:
                break;
        }
    }
    return zparams;
}
