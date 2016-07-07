#include <stdlib.h>    // exit
#include <stdio.h>     // printf
#include <string.h>    // strerror
#include <errno.h>     // errno
#include <sys/stat.h>  // stat
#include <zstd.h>


static off_t fsizeX(const char *filename)
{
    struct stat st;
    if (stat(filename, &st) == 0) return st.st_size;
    /* error */
    printf("stat: %s : %s \n", filename, strerror(errno));
    exit(1);
}

static FILE* fopenX(const char *filename, const char *instruction)
{
    FILE* const inFile = fopen(filename, instruction);
    if (inFile) return inFile;
    /* error */
    printf("fopen: %s : %s \n", filename, strerror(errno));
    exit(2);
}

static void* mallocX(size_t size)
{
    void* const buff = malloc(size);
    if (buff) return buff;
    /* error */
    printf("malloc: %s \n", strerror(errno));
    exit(3);
}

static void* loadFileX(const char* fileName, size_t* size)
{
    off_t const buffSize = fsizeX(fileName);
    FILE* const inFile = fopenX(fileName, "rb");
    void* const buffer = mallocX(buffSize);
    size_t const readSize = fread(buffer, 1, buffSize, inFile);
    if (readSize != (size_t)buffSize) {
        printf("fread: %s : %s \n", fileName, strerror(errno));
        exit(4);
    }
    fclose(inFile);
    *size = buffSize;
    return buffer;
}


static const ZSTD_DDict* createDict(const char* dictFileName)
{
    size_t dictSize;
    void* const dictBuffer = loadFileX(dictFileName, &dictSize);
    const ZSTD_DDict* const ddict = ZSTD_createDDict(dictBuffer, dictSize);
    free(dictBuffer);
    return ddict;
}


/* prototype declared here, as it currently is part of experimental section */
unsigned long long ZSTD_getDecompressedSize(const void* src, size_t srcSize);

static void decompress(const char* fname, const ZSTD_DDict* ddict)
{
    size_t cSize;
    void* const cBuff = loadFileX(fname, &cSize);
    unsigned long long const rSize = ZSTD_getDecompressedSize(cBuff, cSize);
    if (rSize==0) {
        printf("%s : original size unknown \n", fname);
        exit(5);
    }
    void* const rBuff = mallocX(rSize);

    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    size_t const dSize = ZSTD_decompress_usingDDict(dctx, rBuff, rSize, cBuff, cSize, ddict);

    if (dSize != rSize) {
        printf("error decoding %s : %s \n", fname, ZSTD_getErrorName(dSize));
        exit(7);
    }

    /* success */
    printf("%25s : %6u -> %7u \n", fname, (unsigned)cSize, (unsigned)rSize);

    ZSTD_freeDCtx(dctx);
    free(rBuff);
    free(cBuff);
}


int main(int argc, const char** argv)
{
    const char* const exeName = argv[0];

    if (argc<3) {
        printf("wrong arguments\n");
        printf("usage:\n");
        printf("%s [FILES] dictionary\n", exeName);
        return 1;
    }

    /* load dictionary only once */
    const char* const dictName = argv[argc-1];
    const ZSTD_DDict* const dictPtr = createDict(dictName);

    int u;
    for (u=1; u<argc-1; u++) decompress(argv[u], dictPtr);

    printf("All %u files decoded. \n", argc-2);
}
