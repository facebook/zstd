/*
  Streaming compression
  Educational program using zstd library
  Copyright (C) Yann Collet 2016

  GPL v2 License

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

  You can contact the author at :
  - zstd homepage : http://www.zstd.net/
*/

#include <stdlib.h>    // malloc, exit
#include <stdio.h>     // fprintf, perror, feof
#include <string.h>    // strerror
#include <errno.h>     // errno
#define ZSTD_STATIC_LINKING_ONLY  // streaming API defined as "experimental" for the time being
#include <zstd.h>      // presumes zstd library is installed


static void* malloc_orDie(size_t size)
{
    void* const buff = malloc(size);
    if (buff) return buff;
    /* error */
    perror("malloc:");
    exit(1);
}

static FILE* fopen_orDie(const char *filename, const char *instruction)
{
    FILE* const inFile = fopen(filename, instruction);
    if (inFile) return inFile;
    /* error */
    perror(filename);
    exit(3);
}

static size_t fread_orDie(void* buffer, size_t sizeToRead, FILE* file)
{
    size_t const readSize = fread(buffer, 1, sizeToRead, file);
    if (readSize == sizeToRead) return readSize;   /* good */
    if (feof(file)) return readSize;   /* good, reached end of file */
    /* error */
    perror("fread");
    exit(4);
}

static size_t fclose_orDie(FILE* file)
{
    if (!fclose(file)) return 0;
    /* error */
    perror("fclose");
    exit(6);
}


static void decompressFile_orDie(const char* fname)
{
    FILE* const fin  = fopen_orDie(fname, "rb");
    size_t const buffInSize = ZSTD_DStreamInSize();;
    void*  const buffIn  = malloc_orDie(buffInSize);
    size_t const buffOutSize = ZSTD_DStreamOutSize();;
    void*  const buffOut = malloc_orDie(buffOutSize);
    size_t read, toRead = buffInSize;

    ZSTD_DStream* const dstream = ZSTD_createDStream();
    if (dstream==NULL) { fprintf(stderr, "ZSTD_createDStream() error \n"); exit(10); }
    size_t const initResult = ZSTD_initDStream(dstream);
    if (ZSTD_isError(initResult)) { fprintf(stderr, "ZSTD_initDStream() error \n"); exit(11); }

    while( (read = fread_orDie(buffIn, toRead, fin)) ) {
        ZSTD_inBuffer input = { buffIn, read, 0 };
        while (input.pos < input.size) {
            ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
            toRead = ZSTD_decompressStream(dstream, &output , &input);
            /* note : data is just "sinked" into buffOut
               a more complete example would write it to disk or stdout */
        }
    }

    fclose_orDie(fin);
    free(buffIn);
    free(buffOut);
}


int main(int argc, const char** argv)
{
    const char* const exeName = argv[0];
    const char* const inFilename = argv[1];

    if (argc!=2) {
        printf("wrong arguments\n");
        printf("usage:\n");
        printf("%s FILE\n", exeName);
        return 1;
    }

    decompressFile_orDie(inFilename);
    printf("%s correctly decoded (in memory). \n", inFilename);

    return 0;
}
