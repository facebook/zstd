/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "data.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

#include <curl/curl.h>

#include "mem.h"
#include "util.h"
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"

/**
 * Data objects
 */

#define REGRESSION_RELEASE(x) \
    "https://github.com/facebook/zstd/releases/download/regression-data/" x

data_t silesia = {
    .url = REGRESSION_RELEASE("silesia.tar.zst"),
    .name = "silesia",
    .type = data_type_dir,
    .xxhash64 = 0x67558ee5506918b4LL,
};

data_t silesia_tar = {
    .url = REGRESSION_RELEASE("silesia.tar.zst"),
    .name = "silesia.tar",
    .type = data_type_file,
    .xxhash64 = 0x67558ee5506918b4LL,
};

static data_t* g_data[] = {
    &silesia,
    &silesia_tar,
    NULL,
};

data_t const* const* data = (data_t const* const*)g_data;

/**
 * data buffer helper functions (documented in header).
 */

data_buffer_t data_buffer_create(size_t const capacity) {
    data_buffer_t buffer = {};

    buffer.data = (uint8_t*)malloc(capacity);
    if (buffer.data == NULL)
        return buffer;
    buffer.capacity = capacity;
    return buffer;
}

data_buffer_t data_buffer_read(char const* filename) {
    data_buffer_t buffer = {};

    uint64_t const size = UTIL_getFileSize(filename);
    if (size == UTIL_FILESIZE_UNKNOWN) {
        fprintf(stderr, "unknown size for %s\n", filename);
        return buffer;
    }

    buffer.data = (uint8_t*)malloc(size);
    if (buffer.data == NULL) {
        fprintf(stderr, "malloc failed\n");
        return buffer;
    }
    buffer.capacity = size;

    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "file null\n");
        goto err;
    }
    buffer.size = fread(buffer.data, 1, buffer.capacity, file);
    fclose(file);
    if (buffer.size != buffer.capacity) {
        fprintf(stderr, "read %zu != %zu\n", buffer.size, buffer.capacity);
        goto err;
    }

    return buffer;
err:
    free(buffer.data);
    memset(&buffer, 0, sizeof(buffer));
    return buffer;

}

data_buffer_t data_buffer_get(data_t const* data) {
    data_buffer_t const kEmptyBuffer = {};

    if (data->type != data_type_file)
        return kEmptyBuffer;

    return data_buffer_read(data->path);
}

int data_buffer_compare(data_buffer_t buffer1, data_buffer_t buffer2) {
    size_t const size =
        buffer1.size < buffer2.size ? buffer1.size : buffer2.size;
    int const cmp = memcmp(buffer1.data, buffer2.data, size);
    if (cmp != 0)
        return cmp;
    if (buffer1.size < buffer2.size)
        return -1;
    if (buffer1.size == buffer2.size)
        return 0;
    assert(buffer1.size > buffer2.size);
    return 1;

}

void data_buffer_free(data_buffer_t buffer) {
    free(buffer.data);
}

/**
 * Initialization and download functions.
 */

static char* g_data_dir = NULL;

/* mkdir -p */
static int ensure_directory_exists(char const* indir) {
    char* const dir = strdup(indir);
    char* end = dir;
    int ret = 0;
    if (dir == NULL) {
        ret = EINVAL;
        goto out;
    }
    do {
        /* Find the next directory level. */
        for (++end; *end != '\0' && *end != '/'; ++end)
            ;
        /* End the string there, make the directory, and restore the string. */
        char const save = *end;
        *end = '\0';
        int const isdir = UTIL_isDirectory(dir);
        ret = mkdir(dir, S_IRWXU);
        *end = save;
        /* Its okay if the directory already exists. */
        if (ret == 0 || (errno == EEXIST && isdir))
            continue;
        ret = errno;
        fprintf(stderr, "mkdir() failed\n");
        goto out;
    } while (*end != '\0');

    ret = 0;
out:
    free(dir);
    return ret;
}

/** Concatenate 3 strings into a new buffer. */
static char* cat3(char const* str1, char const* str2, char const* str3) {
    size_t const size1 = strlen(str1);
    size_t const size2 = strlen(str2);
    size_t const size3 = strlen(str3);
    size_t const size = size1 + size2 + size3 + 1;
    char* const dst = (char*)malloc(size);
    if (dst == NULL)
        return NULL;
    strcpy(dst, str1);
    strcpy(dst + size1, str2);
    strcpy(dst + size1 + size2, str3);
    assert(strlen(dst) == size1 + size2 + size3);
    return dst;
}

/**
 * State needed by the curl callback.
 * It takes data from curl, hashes it, and writes it to the file.
 */
typedef struct {
    FILE* file;
    XXH64_state_t xxhash64;
    int error;
} curl_data_t;

/** Create the curl state. */
static curl_data_t curl_data_create(data_t const* data) {
    curl_data_t cdata = {};

    XXH64_reset(&cdata.xxhash64, 0);

    assert(UTIL_isDirectory(g_data_dir));

    if (data->type == data_type_file) {
        /* Decompress the resource and store to the path. */
        char* cmd = cat3("zstd -dqfo '", data->path, "'");
        if (cmd == NULL) {
            cdata.error = ENOMEM;
            return cdata;
        }
        cdata.file = popen(cmd, "w");
        free(cmd);
    } else {
        /* Decompress and extract the resource to the cache directory. */
        char* cmd = cat3("zstd -dc | tar -x -C '", g_data_dir, "'");
        if (cmd == NULL) {
            cdata.error = ENOMEM;
            return cdata;
        }
        cdata.file = popen(cmd, "w");
        free(cmd);
    }
    if (cdata.file == NULL) {
        cdata.error = errno;
    }

    return cdata;
}

/** Free the curl state. */
static int curl_data_free(curl_data_t cdata) {
    return pclose(cdata.file);
}

/** curl callback. Updates the hash, and writes to the file. */
static size_t curl_write(void* data, size_t size, size_t count, void* ptr) {
    curl_data_t* cdata = (curl_data_t*)ptr;
    size_t const written = fwrite(data, size, count, cdata->file);
    XXH64_update(&cdata->xxhash64, data, written * size);
    return written;
}

/** Download a single data object. */
static int curl_download_datum(CURL* curl, data_t const* data) {
    curl_data_t cdata = curl_data_create(data);
    int err = EFAULT;

    if (cdata.error != 0) {
        err = cdata.error;
        goto out;
    }

    /* Download the data. */
    if (curl_easy_setopt(curl, CURLOPT_URL, data->url) != 0)
        goto out;
    if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cdata) != 0)
        goto out;
    if (curl_easy_perform(curl) != 0) {
        fprintf(stderr, "downloading '%s' failed\n", data->url);
        goto out;
    }
    /* check that the file exists. */
    if (data->type == data_type_file && !UTIL_isRegularFile(data->path)) {
        fprintf(stderr, "output file '%s' does not exist\n", data->path);
        goto out;
    }
    if (data->type == data_type_dir && !UTIL_isDirectory(data->path)) {
        fprintf(stderr, "output directory '%s' does not exist\n", data->path);
        goto out;
    }
    /* Check that the hash matches. */
    if (XXH64_digest(&cdata.xxhash64) != data->xxhash64) {
        fprintf(
            stderr,
            "checksum does not match: %llx != %llx\n",
            (unsigned long long)XXH64_digest(&cdata.xxhash64),
            (unsigned long long)data->xxhash64);
        goto out;
    }

    err = 0;
out:
    if (err != 0)
        fprintf(stderr, "downloading '%s' failed\n", data->name);
    int const close_err = curl_data_free(cdata);
    if (close_err != 0 && err == 0) {
        fprintf(stderr, "failed to write data for '%s'\n", data->name);
        err = close_err;
    }
    return err;
}

/** Download all the data. */
static int curl_download_data(data_t const* const* data) {
    if (curl_global_init(CURL_GLOBAL_ALL) != 0)
        return EFAULT;

    curl_data_t cdata = {};
    CURL* curl = curl_easy_init();
    int err = EFAULT;

    if (curl == NULL)
        return EFAULT;

    if (curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L) != 0)
        goto out;
    if (curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L) != 0)
        goto out;
    if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write) != 0)
        goto out;

    assert(data != NULL);
    for (; *data != NULL; ++data) {
        if (curl_download_datum(curl, *data) != 0)
            goto out;
    }

    err = 0;
out:
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return err;
}

/** Fill the path member variable of the data objects. */
static int data_create_paths(data_t* const* data, char const* dir) {
    size_t const dirlen = strlen(dir);
    assert(data != NULL);
    for (; *data != NULL; ++data) {
        data_t* const datum = *data;
        datum->path = cat3(dir, "/", datum->name);
        if (datum->path == NULL)
            return ENOMEM;
    }
    return 0;
}

/** Free the path member variable of the data objects. */
static void data_free_paths(data_t* const* data) {
    assert(data != NULL);
    for (; *data != NULL; ++data) {
        data_t* datum = *data;
        free((void*)datum->path);
        datum->path = NULL;
    }
}

static char const kStampName[] = "STAMP";

static void xxh_update_le(XXH64_state_t* state, uint64_t data) {
    if (!MEM_isLittleEndian())
        data = MEM_swap64(data);
    XXH64_update(state, &data, sizeof(data));
}

/** Hash the data to create the stamp. */
static uint64_t stamp_hash(data_t const* const* data) {
    XXH64_state_t state;

    XXH64_reset(&state, 0);
    assert(data != NULL);
    for (; *data != NULL; ++data) {
        data_t const* datum = *data;
        /* We don't care about the URL that we fetch from. */
        /* The path is derived from the name. */
        XXH64_update(&state, datum->name, strlen(datum->name));
        xxh_update_le(&state, datum->xxhash64);
        xxh_update_le(&state, datum->type);
    }
    return XXH64_digest(&state);
}

/** Check if the stamp matches the stamp in the cache directory. */
static int stamp_check(char const* dir, data_t const* const* data) {
    char* stamp = cat3(dir, "/", kStampName);
    uint64_t const expected = stamp_hash(data);
    XXH64_canonical_t actual;
    FILE* stampfile = NULL;
    int matches = 0;

    if (stamp == NULL)
        goto out;
    if (!UTIL_isRegularFile(stamp)) {
        fprintf(stderr, "stamp does not exist: recreating the data cache\n");
        goto out;
    }

    stampfile = fopen(stamp, "rb");
    if (stampfile == NULL) {
        fprintf(stderr, "could not open stamp: recreating the data cache\n");
        goto out;
    }

    size_t b;
    if ((b = fread(&actual, sizeof(actual), 1, stampfile)) != 1) {
        fprintf(stderr, "invalid stamp: recreating the data cache\n");
        goto out;
    }

    matches = (expected == XXH64_hashFromCanonical(&actual));
    if (matches)
        fprintf(stderr, "stamp matches: reusing the cached data\n");
    else
        fprintf(stderr, "stamp does not match: recreating the data cache\n");

out:
    free(stamp);
    if (stampfile != NULL)
        fclose(stampfile);
    return matches;
}

/** On success write a new stamp, on failure delete the old stamp. */
static int
stamp_write(char const* dir, data_t const* const* data, int const data_err) {
    char* stamp = cat3(dir, "/", kStampName);
    FILE* stampfile = NULL;
    int err = EIO;

    if (stamp == NULL)
        return ENOMEM;

    if (data_err != 0) {
        err = data_err;
        goto out;
    }
    XXH64_canonical_t hash;

    XXH64_canonicalFromHash(&hash, stamp_hash(data));

    stampfile = fopen(stamp, "wb");
    if (stampfile == NULL)
        goto out;
    if (fwrite(&hash, sizeof(hash), 1, stampfile) != 1)
        goto out;
    err = 0;
    fprintf(stderr, "stamped new data cache\n");
out:
    if (err != 0)
        /* Ignore errors. */
        unlink(stamp);
    free(stamp);
    if (stampfile != NULL)
        fclose(stampfile);
    return err;
}

int data_init(char const* dir) {
    int err;

    if (dir == NULL)
        return EINVAL;

    /* This must be first to simplify logic. */
    err = ensure_directory_exists(dir);
    if (err != 0)
        return err;

    /* Save the cache directory. */
    g_data_dir = strdup(dir);
    if (g_data_dir == NULL)
        return ENOMEM;

    err = data_create_paths(g_data, dir);
    if (err != 0)
        return err;

    /* If the stamp matches then we are good to go.
     * This must be called before any modifications to the data cache.
     * After this point, we MUST call stamp_write() to update the STAMP,
     * since we've updated the data cache.
     */
    if (stamp_check(dir, data))
        return 0;

    err = curl_download_data(data);
    if (err != 0)
        goto out;

out:
    /* This must be last, since it must know if data_init() succeeded. */
    stamp_write(dir, data, err);
    return err;
}

void data_finish(void) {
    data_free_paths(g_data);
    free(g_data_dir);
    g_data_dir = NULL;
}
