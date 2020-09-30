#include <stddef.h>
#include <stdint.h>

#include "zstd_seekable.h"

static const size_t compressed_size = 17;
static const uint8_t compressed_data[17] = {
    '^',
    '*',
    'M',
    '\x18',
    '\t',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    ';',
    (uint8_t)('\xb1'),
    (uint8_t)('\xea'),
    (uint8_t)('\x92'),
    (uint8_t)('\x8f'),
};

static const size_t uncompressed_size = 32;
static uint8_t uncompressed_data[32] = {
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
    '\x00',
};

int main(int argc, const char** argv)
{
    ZSTD_seekable* stream = ZSTD_seekable_create();
    size_t status = ZSTD_seekable_initBuff(stream, compressed_data, compressed_size);
    if (ZSTD_isError(status)) {
        ZSTD_seekable_free(stream);
        return status;
    }

    const size_t offset = 2;
    status = ZSTD_seekable_decompress(stream, uncompressed_data, uncompressed_size, offset);
    if (ZSTD_isError(status)) {
        ZSTD_seekable_free(stream);
        return status;
    }

    ZSTD_seekable_free(stream);
    return 0;
}
