#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "zstd_errors.h"
#include "matchfinder.h" // simpleExternalMatchFinder, simpleExternalMatchStateDestructor

const size_t ZSTD_LEVEL = 1;

int main(int argc, char *argv[]) {
    size_t res;

    if (argc != 2) {
        printf("Usage: exampleMatchfinder <file>\n");
        return 1;
    }

    ZSTD_CCtx* zc = ZSTD_createCCtx();

    // Here is the crucial bit of code!
    res = ZSTD_registerExternalMatchFinder(
        zc,
        NULL,
        simpleExternalMatchFinder,
        simpleExternalMatchStateDestructor
    );

    if (ZSTD_isError(res)) {
        printf("ERROR: %s\n", ZSTD_getErrorName(res));
        return 1;
    }

    res = ZSTD_CCtx_setParameter(zc, ZSTD_c_useExternalMatchfinder, 1);

    if (ZSTD_isError(res)) {
        printf("ERROR: %s\n", ZSTD_getErrorName(res));
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long srcSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *src = malloc(srcSize + 1);
    fread(src, srcSize, 1, f);
    fclose(f);

    size_t dstSize = ZSTD_compressBound(srcSize);
    char *dst = malloc(dstSize);

    size_t cSize = ZSTD_compress2(zc, dst, dstSize, src, srcSize);

    if (ZSTD_isError(cSize)) {
        printf("ERROR: %s\n", ZSTD_getErrorName(cSize));
        return 1;
    }

    char *val = malloc(srcSize);
    res = ZSTD_decompress(val, srcSize, dst, cSize);

    ZSTD_freeCCtx(zc);

    if (ZSTD_isError(res)) {
        printf("ERROR: %s\n", ZSTD_getErrorName(res));
        return 1;
    }

    if (memcmp(src, val, srcSize) == 0) {
        printf("Compression and decompression were successful!\n");
        printf("Original size: %lu\n", srcSize);
        printf("Compressed size: %lu\n", cSize);
        return 0;
    } else {
        printf("ERROR: input and validation buffers don't match!\n");
        for (int i = 0; i < srcSize; i++) {
            if (src[i] != val[i]) {
                printf("First bad index: %d\n", i);
                break;
            }
        }
        return 1;
    }
}
