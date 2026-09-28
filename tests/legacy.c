/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/*
    This program uses hard-coded data compressed with Zstd legacy versions
    and tests that the API decompresses them correctly
*/

/*===========================================
*   Dependencies
*==========================================*/
#include <stddef.h>              /* size_t */
#include <stdlib.h>              /* malloc, free */
#include <stdio.h>               /* fprintf */
#include <string.h>              /* strlen, memcpy, memcmp */
#define ZSTD_STATIC_LINKING_ONLY /* ZSTD_decompressBound */
#include "zstd.h"
#include "zstd_errors.h"

/* Same normalization as lib/legacy/zstd_legacy.h :
 * no legacy support means only the current format can be decoded */
#if !defined(ZSTD_LEGACY_SUPPORT) || (ZSTD_LEGACY_SUPPORT == 0)
#  undef ZSTD_LEGACY_SUPPORT
#  define ZSTD_LEGACY_SUPPORT 8
#endif

/*===========================================
*   Macros
*==========================================*/
#define DISPLAY(...)          fprintf(stderr, __VA_ARGS__)

/*===========================================
*   Precompressed frames
*==========================================*/
/* Each frame below is the "BLOCK" text, compressed with default settings by
 * the last release able to produce that format version.
 * A frame produced by v0.N is only decodable when ZSTD_LEGACY_SUPPORT <= N,
 * hence the test adapts itself to the level it has been compiled with.
 */
#define FRAME_V01_SIZE 189
#define FRAME_V02_SIZE 187
#define FRAME_V03_SIZE 187
#define FRAME_V04_SIZE 198
#define FRAME_V05_SIZE 184
#define FRAME_V06_SIZE 178
#define FRAME_V07_SIZE 178
#define FRAME_V08_SIZE 179
#define ALL_FRAMES_SIZE (FRAME_V01_SIZE + FRAME_V02_SIZE + FRAME_V03_SIZE + FRAME_V04_SIZE + FRAME_V05_SIZE + FRAME_V06_SIZE + FRAME_V07_SIZE + FRAME_V08_SIZE)

const char* const FRAME_V01;   /* content is at end of file, produced by v0.1.1 */
const char* const FRAME_V02;   /* content is at end of file, produced by v0.2.2 */
const char* const FRAME_V03;   /* content is at end of file, produced by v0.3.6 */
const char* const FRAME_V04;   /* content is at end of file, produced by v0.4.3 */
const char* const FRAME_V05;   /* content is at end of file, produced by v0.5.0 */
const char* const FRAME_V06;   /* content is at end of file, produced by v0.6.0 */
const char* const FRAME_V07;   /* content is at end of file, produced by v0.7.0 */
const char* const FRAME_V08;   /* content is at end of file, produced by v0.8.0 */
const char* const BLOCK;      /* content is at end of file */


/* legacy formats older than v0.4 only offer a single-shot decoder :
 * ZSTD_decompressStream() answers version_unsupported on such frames
 * (see ZSTD_initLegacyStream() in lib/legacy/zstd_legacy.h) */
#define OLDEST_STREAMABLE_VERSION 4

/* buildTestFrames() :
 * concatenates, oldest first, the frames this build is able to decode,
 * skipping those older than @fromVersion.
 * @dst must be at least ALL_FRAMES_SIZE bytes long.
 * @return : nb of frames written; *cSize receives their total size */
static unsigned buildTestFrames(char* dst, size_t* cSize, int fromVersion)
{
    unsigned nbFrames = 0;
    size_t pos = 0;

#define ADD_FRAME(n, v)                                     \
    if (fromVersion <= (n)) {                               \
        memcpy(dst + pos, FRAME_V##v, FRAME_V##v##_SIZE);   \
        pos += FRAME_V##v##_SIZE;                           \
        nbFrames++;                                         \
    }

#if (ZSTD_LEGACY_SUPPORT <= 1)
    ADD_FRAME(1, 01)
#endif
#if (ZSTD_LEGACY_SUPPORT <= 2)
    ADD_FRAME(2, 02)
#endif
#if (ZSTD_LEGACY_SUPPORT <= 3)
    ADD_FRAME(3, 03)
#endif
#if (ZSTD_LEGACY_SUPPORT <= 4)
    ADD_FRAME(4, 04)
#endif
#if (ZSTD_LEGACY_SUPPORT <= 5)
    ADD_FRAME(5, 05)
#endif
#if (ZSTD_LEGACY_SUPPORT <= 6)
    ADD_FRAME(6, 06)
#endif
#if (ZSTD_LEGACY_SUPPORT <= 7)
    ADD_FRAME(7, 07)
#endif
    ADD_FRAME(8, 08)
#undef ADD_FRAME

    *cSize = pos;
    return nbFrames;
}


static int testSimpleAPI(const char* compressed, size_t compressedSize,
                         const char* expected, size_t expectedSize)
{
    char* const output = malloc(expectedSize);

    if (!output) {
        DISPLAY("ERROR: Not enough memory!\n");
        return 1;
    }

    {
        size_t const ret = ZSTD_decompress(output, expectedSize, compressed, compressedSize);
        if (ZSTD_isError(ret)) {
            if (ret == ZSTD_error_prefix_unknown) {
                DISPLAY("ERROR: Invalid frame magic number, was this compiled "
                        "without legacy support?\n");
            } else {
                DISPLAY("ERROR: %s\n", ZSTD_getErrorName(ret));
            }
            free(output);
            return 1;
        }
        if (ret != expectedSize) {
            DISPLAY("ERROR: Wrong decoded size\n");
            free(output);
            return 1;
        }
    }
    if (memcmp(expected, output, expectedSize) != 0) {
        DISPLAY("ERROR: Wrong decoded output produced\n");
        free(output);
        return 1;
    }

    free(output);
    DISPLAY("Simple API OK\n");
    return 0;
}


static int testStreamingAPI(const char* compressed, size_t compressedSize,
                            const char* expected, size_t expectedSize)
{
    int error_code = 0;
    size_t const outBuffSize = ZSTD_DStreamOutSize();
    char* const outBuff = malloc(outBuffSize);
    ZSTD_DStream* const stream = ZSTD_createDStream();
    ZSTD_inBuffer input = { NULL, 0, 0 };
    size_t outputPos = 0;
    int needsInit = 1;

    input.src = compressed;
    input.size = compressedSize;

    if (outBuff == NULL) {
        DISPLAY("ERROR: Could not allocate memory\n");
        return 1;
    }
    if (stream == NULL) {
        DISPLAY("ERROR: Could not create dstream\n");
        free(outBuff);
        return 1;
    }

    while (1) {
        ZSTD_outBuffer output = {outBuff, outBuffSize, 0};
        if (needsInit) {
            size_t const ret = ZSTD_initDStream(stream);
            if (ZSTD_isError(ret)) {
                DISPLAY("ERROR: ZSTD_initDStream: %s\n", ZSTD_getErrorName(ret));
                error_code = 1;
                break;
        }   }

        {   size_t const ret = ZSTD_decompressStream(stream, &output, &input);
            if (ZSTD_isError(ret)) {
                DISPLAY("ERROR: ZSTD_decompressStream: %s\n", ZSTD_getErrorName(ret));
                error_code = 1;
                break;
            }

            if (ret == 0) {
                needsInit = 1;
        }   }

        if (outputPos + output.pos > expectedSize) {
            DISPLAY("ERROR: Decoded more than expected\n");
            error_code = 1;
            break;
        }
        if (memcmp(outBuff, expected + outputPos, output.pos) != 0) {
            DISPLAY("ERROR: Wrong decoded output produced\n");
            error_code = 1;
            break;
        }
        outputPos += output.pos;
        if (input.pos == input.size && output.pos < output.size) {
            break;
        }
    }

    if (error_code == 0 && outputPos != expectedSize) {
        DISPLAY("ERROR: Decoded less than expected\n");
        error_code = 1;
    }

    free(outBuff);
    ZSTD_freeDStream(stream);
    if (error_code == 0) DISPLAY("Streaming API OK\n");
    return error_code;
}

static int testFrameDecoding(const char* compressed, size_t compressedSize,
                             size_t expectedSize)
{
    if (expectedSize > ZSTD_decompressBound(compressed, compressedSize)) {
        DISPLAY("ERROR: ZSTD_decompressBound: decompressed bound too small\n");
        return 1;
    }
    {   const char* ip = compressed;
        size_t remainingSize = compressedSize;
        while (1) {
            size_t frameSize = ZSTD_findFrameCompressedSize(ip, remainingSize);
            if (ZSTD_isError(frameSize)) {
                DISPLAY("ERROR: ZSTD_findFrameCompressedSize: %s\n", ZSTD_getErrorName(frameSize));
                return 1;
            }
            if (frameSize > remainingSize) {
                DISPLAY("ERROR: ZSTD_findFrameCompressedSize: expected frameSize to align with src buffer");
                return 1;
            }
            ip += frameSize;
            remainingSize -= frameSize;
            if (remainingSize == 0) break;
        }
    }
    DISPLAY("Frame Decoding OK\n");
    return 0;
}

int main(void)
{
    size_t const blockSize = strlen(BLOCK);
    char* const compressed = malloc(ALL_FRAMES_SIZE);
    /* the streaming API can only replay the subset of frames for which a
     * legacy streaming decoder exists : it gets a buffer of its own, so that
     * each buffer keeps matching the size recorded next to it */
    char* const streamable = malloc(ALL_FRAMES_SIZE);
    char* expected = NULL;
    size_t compressedSize = 0;
    size_t streamableSize = 0;
    size_t expectedSize = 0;
    unsigned nbFrames = 0;
    unsigned nbStreamFrames = 0;
    int result = 1;

    do {
        if (compressed == NULL || streamable == NULL) {
            DISPLAY("ERROR: Not enough memory!\n");
            break;
        }
        nbFrames = buildTestFrames(compressed, &compressedSize, 1);
        nbStreamFrames = buildTestFrames(streamable, &streamableSize, OLDEST_STREAMABLE_VERSION);
        DISPLAY("Testing %u frames, from format v0.%i upward\n", nbFrames, ZSTD_LEGACY_SUPPORT);

        expectedSize = blockSize * nbFrames;
        expected = malloc(expectedSize);
        if (expected == NULL) {
            DISPLAY("ERROR: Not enough memory!\n");
            break;
        }
        {   unsigned u;
            for (u = 0; u < nbFrames; u++)
                memcpy(expected + (u * blockSize), BLOCK, blockSize);
        }

        if (testSimpleAPI(compressed, compressedSize, expected, expectedSize)) break;
        if (testFrameDecoding(compressed, compressedSize, expectedSize)) break;
        /* all blocks being identical, the first @nbStreamFrames ones
         * are what the streaming subset must decode to */
        if (testStreamingAPI(streamable, streamableSize,
                             expected, blockSize * nbStreamFrames)) break;
        result = 0;
    } while (0);

    free(expected);
    free(streamable);
    free(compressed);
    if (result == 0) DISPLAY("OK\n");
    return result;
}

const char* const FRAME_V01 =
    "\xFD\x2F\xB5\x1E\x00\x00\xB3\x00\x00\x9D\x00\xD8\x1F\xB0\x01\x10"
    "\x00\x00\x00\xA0\xCA\x4C\xCF\x43\xDF\xF9\xF8\x7B\x7F\xFE\x3F\xDF"
    "\xDD\xDF\xC3\x86\x55\x63\x3A\x68\x70\x44\x44\x08\x1F\x00\x1D\x00"
    "\x1D\x00\x44\x21\x80\x32\xAD\x75\x6F\xB1\x6B\x3B\x7A\x3F\x77\xAE"
    "\x8F\x95\x4A\x73\xEC\xCF\x73\x0C\x07\x9A\x9E\xF0\x5E\xB4\xCB\x94"
    "\x06\x99\xE6\x3E\x28\xF0\x80\x42\xA9\x1A\x2B\x0E\xDF\xC9\x06\x82"
    "\xB6\x56\x72\x98\x49\xB2\xBE\xEE\xCD\x25\xA1\xAC\x86\x01\x99\xD4"
    "\x28\x05\x9F\xC8\xF9\x1D\x90\x30\x6B\x18\x08\x9B\xA0\xF5\x12\xC9"
    "\x5F\x4B\x80\x37\xAD\x14\x5A\xD2\xCD\x98\x01\xE8\x77\x03\x63\x08"
    "\xB4\xA4\x81\xD8\xE9\x24\xB2\x73\x9F\x9C\x03\x07\x52\x6A\x2F\xC2"
    "\xCE\x70\x1B\xFF\xD0\x4E\x01\x04\x00\x54\x01\x43\x80\x07\x00\xA1"
    "\x28\x3F\x20\x9C\x6A\x30\xA3\x01\xC8\x12\xC0\x00\x00";

const char* const FRAME_V02 =
    "\x22\xB5\x2F\xFD\x00\x00\xB1\x60\x03\x20\x13\x00\x1F\xB0\x01\x10"
    "\x00\x00\x00\xA0\xCA\x4C\xCF\x43\xDF\xF9\xF8\x7B\x7F\xFE\x3F\xDF"
    "\xDD\xDF\xC3\x86\x55\x63\x3A\x68\x70\x44\x44\x08\x19\x00\x1D\x00"
    "\x1E\x00\xAB\x65\x8D\x76\xE9\x32\x5A\x7D\xA3\x19\xED\xF6\x94\xB8"
    "\x12\xEC\x2E\xA3\xB9\x6C\x35\xA3\x55\x4A\xD4\xC9\x68\x56\x3B\x79"
    "\xDB\x40\xEA\x07\x52\x23\x99\x5C\x3E\xB2\xB6\xB3\x6B\xCE\x96\x18"
    "\x2A\xF3\x20\x00\x0A\xED\xED\x36\xB5\xB1\x2E\x72\xBA\x1D\xCC\x19"
    "\xED\x12\x51\x9C\x63\x0E\xE7\x5E\xC8\xB8\xCD\xF0\x39\x84\xC4\x6E"
    "\x2F\x27\x46\x8D\x90\x0C\x44\x21\x80\x32\xAD\x28\xB3\x4E\x79\xAB"
    "\x79\x8D\xDB\xCB\x85\x41\xC1\xC1\x9A\x8B\x35\xA3\x11\xE4\xAB\x48"
    "\xC0\xE5\x02\xCA\x0C\x04\x00\x54\x01\x43\x80\x07\x00\xA1\x28\x3F"
    "\x20\x9C\x6A\x30\xA3\x01\xC8\x12\xC0\x00\x00";

const char* const FRAME_V03 =
    "\x23\xB5\x2F\xFD\x00\x00\xB1\x50\x03\xE0\x12\x00\x1F\xB0\x01\x10"
    "\x00\x00\x00\xA0\xCA\x4C\xCF\x43\xDF\xF9\xF8\x7B\x7F\xFE\x3F\xDF"
    "\xDD\xDF\xC3\x86\x55\x63\x3A\x68\x70\x44\x44\x08\x19\x00\x1D\x00"
    "\x1D\x00\x6A\x59\xA3\x5D\xBA\x8C\x56\xDF\x68\x46\xBB\x3D\x25\xAE"
    "\x04\xBB\xCB\x68\x2E\x5B\xCD\x68\x95\x12\x35\x48\x26\xA3\x59\xED"
    "\xE4\x6D\x03\xA9\x1F\x48\x8D\x64\x72\xF9\xC8\xDA\xCE\xAE\x39\x5B"
    "\x62\xA8\xCC\x83\x00\x28\xB4\x0F\x59\x1B\xEB\x22\xA7\xDB\xC1\x9C"
    "\xD1\x2E\x11\xC5\x39\xE6\x70\xEE\x85\x8C\xDB\x0C\x9F\x43\x48\xEC"
    "\xF6\x72\x62\xD4\x28\x44\x21\x80\x32\xAD\x28\xB3\x4E\x79\xAB\x79"
    "\x8D\xDB\xCB\x85\x41\xC1\xC1\x9A\x8B\x35\xA3\x11\xE4\xAB\x48\xC0"
    "\xE5\x02\xCA\x05\x00\x54\x01\x43\x80\x07\x00\xA1\x28\x3F\xE0\x04"
    "\x80\x00\xA0\xC1\x8C\x06\x20\x4B\xC0\x00\x00";

const char* const FRAME_V04 =
    "\x24\xB5\x2F\xFD\x00\x00\x00\xBB\xB0\x02\xC0\x10\x00\x1E\xB0\x01"
    "\x02\x00\x00\x80\x00\xE8\x92\x34\x12\x97\xC8\xDF\xE9\xF3\xEF\x53"
    "\xEA\x1D\x27\x4F\x0C\x44\x90\x0C\x8D\xF1\xB4\x89\x17\x00\x18\x00"
    "\x18\x00\x3F\xE6\xE2\xE3\x74\xD6\xEC\xC9\x4A\xE0\x71\x71\x42\x3E"
    "\x64\x4F\x6A\x45\x4E\x78\xEC\x49\x03\x3F\xC6\x80\xAB\x8F\x75\x5E"
    "\x6F\x2E\x3E\x7E\xC6\xDC\x45\x69\x6C\xC5\xFD\xC7\x40\xB8\x84\x8A"
    "\x01\xEB\xA8\xD1\x40\x39\x90\x4C\x64\xF8\xEB\x53\xE6\x18\x0B\x67"
    "\x12\xAD\xB8\x99\xB3\x5A\x6F\x8A\x19\x03\x01\x50\x67\x56\xF5\x9F"
    "\x35\x84\x60\xA0\x60\x91\xC9\x0A\xDC\xAB\xAB\xE0\xE2\x81\xFA\xCF"
    "\xC6\xBA\x01\x0E\x00\x54\x00\x00\x19\x00\x00\x54\x14\x00\x24\x24"
    "\x04\xFE\x04\x84\x4E\x41\x00\x27\xE2\x02\xC4\xB1\x00\xD2\x51\x00"
    "\x79\x58\x41\x28\x00\xE0\x0C\x01\x68\x65\x00\x04\x13\x0C\xDA\x0C"
    "\x80\x22\x06\xC0\x00\x00";

const char* const FRAME_V05 =
    "\x25\xB5\x2F\xFD\x00\x00\x00\xAD\x12\xB0\x7D\x1E\xB0\x01\x02\x00"
    "\x00\x80\x00\xE8\x92\x34\x12\x97\xC8\xDF\xE9\xF3\xEF\x53\xEA\x1D"
    "\x27\x4F\x0C\x44\x90\x0C\x8D\xF1\xB4\x89\x03\x01\x50\x67\x56\xF5"
    "\x9F\x35\x84\x60\xA0\x60\x91\xC9\x0A\xDC\xAB\xAB\xE0\xE2\x81\xFA"
    "\xCF\xC6\xBA\xEB\xA8\xD1\x40\x39\x90\x4C\x64\xF8\xEB\x53\xE6\x18"
    "\x0B\x67\x12\xAD\xB8\x99\xB3\x5A\x6F\x8A\xF9\x63\x0C\xB8\xFA\x58"
    "\xE7\xF5\xE6\xE2\xE3\x67\xCC\x5D\x94\xC6\x56\xDC\x7F\x0C\x84\x4B"
    "\xA8\xF8\x63\x2E\x3E\x4E\x67\xCD\x9E\xAC\x04\x1E\x17\x27\xE4\x43"
    "\xF6\xA4\x56\xE4\x84\xC7\x9E\x34\x0E\x00\x00\x32\x40\x80\xA8\x00"
    "\x01\x49\x81\xE0\x3C\x01\x29\x1D\x00\x87\xCE\x80\x75\x08\x80\x72"
    "\x24\x00\x7B\x52\x00\x94\x00\x20\xCC\x01\x86\xD2\x00\x81\x09\x83"
    "\xC1\x34\xA0\x88\x01\xC0\x00\x00";

const char* const FRAME_V06 =
    "\x26\xB5\x2F\xFD\x42\xEF\x00\x00\xA6\x12\xB0\x7D\x1E\xB0\x01\x02"
    "\x00\x00\x54\xA0\xBA\x24\x8D\xC4\x25\xF2\x77\xFA\xFC\xFB\x94\x7A"
    "\xC7\xC9\x13\x03\x11\x24\x43\x63\x3C\x6D\x22\x03\x01\x50\x67\x56"
    "\xF5\x9F\x35\x84\x60\xA0\x60\x91\xC9\x0A\xDC\xAB\xAB\xE0\xE2\x81"
    "\xFA\xCF\xC6\xBA\xEB\xA8\xD1\x40\x39\x90\x4C\x64\xF8\xEB\x53\xE6"
    "\x18\x0B\x67\x12\xAD\xB8\x99\xB3\x5A\x6F\x8A\xF9\x63\x0C\xB8\xFA"
    "\x58\xE7\xF5\xE6\xE2\xE3\x67\xCC\x5D\x94\xC6\x56\xDC\x7F\x0C\x84"
    "\x4B\xA8\xF8\x63\x2E\x3E\x4E\x67\xCD\x9E\xAC\x04\x1E\x17\x27\xE4"
    "\x43\xF6\xA4\x56\xE4\x84\xC7\x9E\x34\x0E\x00\x35\x0B\x71\xB5\xC0"
    "\x2A\x5C\x26\x94\x22\x20\x8B\x4C\x8D\x13\x47\x58\x67\x15\x6C\xF1"
    "\x1C\x4B\x54\x10\x9D\x31\x50\x85\x4B\x54\x0E\x01\x4B\x3D\x01\xC0"
    "\x00\x00";

const char* const FRAME_V07 =
    "\x27\xB5\x2F\xFD\x20\xEF\x00\x00\xA6\x12\xE4\x84\x1F\xB0\x01\x10"
    "\x00\x00\x00\x35\x59\xA6\xE7\xA1\xEF\x7C\xFC\xBD\x3F\xFF\x9F\xEF"
    "\xEE\xEF\x61\xC3\xAA\x31\x1D\x34\x38\x22\x22\x04\x44\x21\x80\x32"
    "\xAD\x28\xF3\xD6\x28\x0C\x0A\x0E\xD6\x5C\xAC\x19\x8D\x20\x5F\x45"
    "\x02\x2E\x17\x50\x66\x6D\xAC\x8B\x9C\x6E\x07\x73\x46\xBB\x44\x14"
    "\xE7\x98\xC3\xB9\x17\x32\x6E\x33\x7C\x0E\x21\xB1\xDB\xCB\x89\x51"
    "\x23\x34\xAB\x9D\xBC\x6D\x20\xF5\x03\xA9\x91\x4C\x2E\x1F\x59\xDB"
    "\xD9\x35\x67\x4B\x0C\x95\x79\x10\x00\x85\xA6\x96\x95\x2E\xDF\x78"
    "\x7B\x4A\x5C\x09\x76\x97\xD1\x5C\x96\x12\x75\x35\xA3\x55\x4A\xD4"
    "\x0B\x00\x35\x0B\x71\xB5\xC0\x2A\x5C\xE6\x08\x45\xF1\x39\x43\xF1"
    "\x1C\x4B\x54\x10\x9D\x31\x50\x85\x4B\x54\x0E\x01\x4B\x3D\x01\xC0"
    "\x00\x00";

const char* const FRAME_V08 =
    "\x28\xB5\x2F\xFD\x24\xEF\x35\x05\x00\x92\x0B\x21\x1F\xB0\x01\x10"
    "\x00\x00\x00\x35\x59\xA6\xE7\xA1\xEF\x7C\xFC\xBD\x3F\xFF\x9F\xEF"
    "\xEE\xEF\x61\xC3\xAA\x31\x1D\x34\x38\x22\x22\x04\x44\x21\x80\x32"
    "\xAD\x28\xF3\xD6\x28\x0C\x0A\x0E\xD6\x5C\xAC\x19\x8D\x20\x5F\x45"
    "\x02\x2E\x17\x50\x66\x6D\xAC\x8B\x9C\x6E\x07\x73\x46\xBB\x44\x14"
    "\xE7\x98\xC3\xB9\x17\x32\x6E\x33\x7C\x0E\x21\xB1\xDB\xCB\x89\x51"
    "\x23\x34\xAB\x9D\xBC\x6D\x20\xF5\x03\xA9\x91\x4C\x2E\x1F\x59\xDB"
    "\xD9\x35\x67\x4B\x0C\x95\x79\x10\x00\x85\xA6\x96\x95\x2E\xDF\x78"
    "\x7B\x4A\x5C\x09\x76\x97\xD1\x5C\x96\x12\x75\x35\xA3\x55\x4A\xD4"
    "\x0B\x00\x35\x0B\x71\xB5\xC0\x2A\x5C\xE6\x08\x45\xF1\x39\x43\xF1"
    "\x1C\x4B\x54\x10\x9D\x31\x50\x85\x4B\x54\x0E\x01\x4B\x3D\x01\xD2"
    "\x2F\x21\x80";


const char* const BLOCK =
    "snowden is snowed in / he's now then in his snow den / when does the snow end?\n"
    "goodbye little dog / you dug some holes in your day / they'll be hard to fill.\n"
    "when life shuts a door, / just open it. it’s a door. / that is how doors work.\n";
