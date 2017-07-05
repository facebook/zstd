#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)
#define FILE_CHUNK_SIZE 4 << 20
typedef unsigned char BYTE;

#include <stdio.h>
#include <stdlib.h>
#include "zstd.h"



/* return 0 if successful, else return error */
int main(int argCount, const char* argv[])
{
    const char* const srcFilename = argv[1];
    const char* const dstFilename = argv[2];
    FILE* const srcFile = fopen(srcFilename, "rb");
    FILE* const dstFile = fopen(dstFilename, "wb");
    BYTE* const src = malloc(FILE_CHUNK_SIZE);
    size_t const dstSize = ZSTD_compressBound(FILE_CHUNK_SIZE);
    BYTE* const dst = malloc(dstSize);
    int ret = 0;

    /* checking for errors */
    if (!srcFilename || !dstFilename || !src || !dst) {
        DISPLAY("Error: initial variables could not be allocated\n");
        ret = 1;
         goto cleanup;
    }

    /* compressing in blocks */
    for ( ; ; ) {
        size_t const readSize = fread(src, 1, FILE_CHUNK_SIZE, srcFile);
        if (readSize != FILE_CHUNK_SIZE && !feof(srcFile)) {
            DISPLAY("Error: could not read %d bytes\n", FILE_CHUNK_SIZE);
            ret = 1;
            goto cleanup;
        }
        {
            size_t const compressedSize = ZSTD_compress(dst, dstSize, src, readSize, 6);
            if (ZSTD_isError(compressedSize)) {
                DISPLAY("Error: something went wrong during compression\n");
                ret = 1;
                goto cleanup;
            }
            {
                size_t const writeSize = fwrite(dst, 1, compressedSize, dstFile);
                if (writeSize != compressedSize) {
                    DISPLAY("Error: could not write compressed data to file\n");
                    ret = 1;
                    goto cleanup;
                }
            }
        }
        if (feof(srcFile)) {
            /* reached end of file */
            break;
        }
    }

    /* file compression completed */
    {
        int const error = fclose(srcFile);
        if (ret != 0) {
            DISPLAY("Error: could not close the file\n");
            ret = error;
            goto cleanup;
        }
    }
cleanup:
    if (src != NULL) free(src);
    if (dst != NULL) free(dst);
    return ret;
}
