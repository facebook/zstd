/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


/*_************************************
*  Includes
**************************************/
#include "util.h"        /* Compiler options, UTIL_GetFileSize */
#include <stdlib.h>      /* malloc */
#include <stdio.h>       /* fprintf, fopen, ftello64 */
#include <time.h>        /* clock_t, clock, CLOCKS_PER_SEC */

#include "mem.h"
#ifndef ZSTD_DLL_IMPORT
    #include "zstd_internal.h"   /* ZSTD_blockHeaderSize, blockType_e, KB, MB */
    #define ZSTD_STATIC_LINKING_ONLY  /* ZSTD_compressBegin, ZSTD_compressContinue, etc. */
#else
    #define KB *(1 <<10)
    #define MB *(1 <<20)
    #define GB *(1U<<30)
    typedef enum { bt_raw, bt_rle, bt_compressed, bt_reserved } blockType_e; 
#endif
#include "zstd.h"            /* ZSTD_VERSION_STRING */
#include "datagen.h"


/*_************************************
*  Constants
**************************************/
#define PROGRAM_DESCRIPTION "Zstandard speed analyzer"
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %s %i-bits, by %s (%s) ***\n", PROGRAM_DESCRIPTION, ZSTD_VERSION_STRING, (int)(sizeof(void*)*8), AUTHOR, __DATE__

#define NBLOOPS    6
#define TIMELOOP_S 2

#define KNUTH      2654435761U
#define MAX_MEM    (1984 MB)

#define COMPRESSIBILITY_DEFAULT 0.50
static const size_t g_sampleSize = 10000000;


/*_************************************
*  Macros
**************************************/
#define DISPLAY(...)  fprintf(stderr, __VA_ARGS__)


/*_************************************
*  Benchmark Parameters
**************************************/
static U32 g_nbIterations = NBLOOPS;
static double g_compressibility = COMPRESSIBILITY_DEFAULT;

static void BMK_SetNbIterations(U32 nbLoops)
{
    g_nbIterations = nbLoops;
    DISPLAY("- %i iterations -\n", g_nbIterations);
}


/*_*******************************************************
*  Private functions
*********************************************************/
static clock_t BMK_clockSpan( clock_t clockStart )
{
    return clock() - clockStart;   /* works even if overflow, span limited to <= ~30mn */
}


static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t const step = 64 MB;
    void* testmem = NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

    requiredMem += step;
    do {
        testmem = malloc ((size_t)requiredMem);
        requiredMem -= step;
    } while (!testmem);

    free (testmem);
    return (size_t) requiredMem;
}


/*_*******************************************************
*  Benchmark wrappers
*********************************************************/
typedef struct {
    blockType_e blockType;
    U32 unusedBits;
    U32 origSize;
} blockProperties_t;

size_t local_ZSTD_compress(void* dst, size_t dstSize, void* buff2, const void* src, size_t srcSize)
{
    (void)buff2;
    return ZSTD_compress(dst, dstSize, src, srcSize, 1);
}

static size_t g_cSize = 0;
size_t local_ZSTD_decompress(void* dst, size_t dstSize, void* buff2, const void* src, size_t srcSize)
{
    (void)src; (void)srcSize;
    return ZSTD_decompress(dst, dstSize, buff2, g_cSize);
}

#ifndef ZSTD_DLL_IMPORT
static ZSTD_DCtx* g_zdc = NULL;
extern size_t ZSTD_decodeLiteralsBlock(ZSTD_DCtx* ctx, const void* src, size_t srcSize);
size_t local_ZSTD_decodeLiteralsBlock(void* dst, size_t dstSize, void* buff2, const void* src, size_t srcSize)
{
    (void)src; (void)srcSize; (void)dst; (void)dstSize;
    return ZSTD_decodeLiteralsBlock((ZSTD_DCtx*)g_zdc, buff2, g_cSize);
}

extern size_t ZSTD_getcBlockSize(const void* src, size_t srcSize, blockProperties_t* bpPtr);
extern size_t ZSTD_decodeSeqHeaders(ZSTD_DCtx* dctx, int* nbSeq, const void* src, size_t srcSize);
size_t local_ZSTD_decodeSeqHeaders(void* dst, size_t dstSize, void* buff2, const void* src, size_t srcSize)
{
    int nbSeq;
    (void)src; (void)srcSize; (void)dst; (void)dstSize;
    return ZSTD_decodeSeqHeaders(g_zdc, &nbSeq, buff2, g_cSize);
}
#endif

static ZSTD_CStream* g_cstream= NULL;
size_t local_ZSTD_compressStream(void* dst, size_t dstCapacity, void* buff2, const void* src, size_t srcSize)
{
    ZSTD_outBuffer buffOut;
    ZSTD_inBuffer buffIn;
    (void)buff2;
    ZSTD_initCStream(g_cstream, 1);
    buffOut.dst = dst;
    buffOut.size = dstCapacity;
    buffOut.pos = 0;
    buffIn.src = src;
    buffIn.size = srcSize;
    buffIn.pos = 0;
    ZSTD_compressStream(g_cstream, &buffOut, &buffIn);
    ZSTD_endStream(g_cstream, &buffOut);
    return buffOut.pos;
}

static ZSTD_DStream* g_dstream= NULL;
static size_t local_ZSTD_decompressStream(void* dst, size_t dstCapacity, void* buff2, const void* src, size_t srcSize)
{
    ZSTD_outBuffer buffOut;
    ZSTD_inBuffer buffIn;
    (void)src; (void)srcSize;
    ZSTD_initDStream(g_dstream);
    buffOut.dst = dst;
    buffOut.size = dstCapacity;
    buffOut.pos = 0;
    buffIn.src = buff2;
    buffIn.size = g_cSize;
    buffIn.pos = 0;
    ZSTD_decompressStream(g_dstream, &buffOut, &buffIn);
    return buffOut.pos;
}

#ifndef ZSTD_DLL_IMPORT
static ZSTD_CCtx* g_zcc = NULL;
size_t local_ZSTD_compressContinue(void* dst, size_t dstCapacity, void* buff2, const void* src, size_t srcSize)
{
    (void)buff2;
    ZSTD_compressBegin(g_zcc, 1);
    return ZSTD_compressEnd(g_zcc, dst, dstCapacity, src, srcSize);
}

#define FIRST_BLOCK_SIZE 8
size_t local_ZSTD_compressContinue_extDict(void* dst, size_t dstCapacity, void* buff2, const void* src, size_t srcSize)
{
    BYTE firstBlockBuf[FIRST_BLOCK_SIZE];

    (void)buff2;
    memcpy(firstBlockBuf, src, FIRST_BLOCK_SIZE);
    ZSTD_compressBegin(g_zcc, 1);

    {   size_t const compressResult = ZSTD_compressContinue(g_zcc, dst, dstCapacity, firstBlockBuf, FIRST_BLOCK_SIZE);
        if (ZSTD_isError(compressResult)) { DISPLAY("local_ZSTD_compressContinue_extDict error : %s\n", ZSTD_getErrorName(compressResult)); return compressResult; }
        dst = (BYTE*)dst + compressResult;
        dstCapacity -= compressResult;
    }
    return ZSTD_compressEnd(g_zcc, dst, dstCapacity, (const BYTE*)src + FIRST_BLOCK_SIZE, srcSize - FIRST_BLOCK_SIZE);
}

size_t local_ZSTD_decompressContinue(void* dst, size_t dstCapacity, void* buff2, const void* src, size_t srcSize)
{
    size_t regeneratedSize = 0;
    const BYTE* ip = (const BYTE*)buff2;
    const BYTE* const iend = ip + g_cSize;
    BYTE* op = (BYTE*)dst;
    size_t remainingCapacity = dstCapacity;

    (void)src; (void)srcSize;
    ZSTD_decompressBegin(g_zdc);
    while (ip < iend) {
        size_t const iSize = ZSTD_nextSrcSizeToDecompress(g_zdc);
        size_t const decodedSize = ZSTD_decompressContinue(g_zdc, op, remainingCapacity, ip, iSize);
        ip += iSize;
        regeneratedSize += decodedSize;
        op += decodedSize;
        remainingCapacity -= decodedSize;
    }

    return regeneratedSize;
}
#endif


/*_*******************************************************
*  Bench functions
*********************************************************/
static size_t benchMem(const void* src, size_t srcSize, U32 benchNb)
{
    BYTE*  dstBuff;
    size_t const dstBuffSize = ZSTD_compressBound(srcSize);
    void*  buff2;
    const char* benchName;
    size_t (*benchFunction)(void* dst, size_t dstSize, void* verifBuff, const void* src, size_t srcSize);
    double bestTime = 100000000.;

    /* Selection */
    switch(benchNb)
    {
    case 1:
        benchFunction = local_ZSTD_compress; benchName = "ZSTD_compress";
        break;
    case 2:
        benchFunction = local_ZSTD_decompress; benchName = "ZSTD_decompress";
        break;
#ifndef ZSTD_DLL_IMPORT
    case 11:
        benchFunction = local_ZSTD_compressContinue; benchName = "ZSTD_compressContinue";
        break;
    case 12:
        benchFunction = local_ZSTD_compressContinue_extDict; benchName = "ZSTD_compressContinue_extDict";
        break;
    case 13:
        benchFunction = local_ZSTD_decompressContinue; benchName = "ZSTD_decompressContinue";
        break;
	case 31:
        benchFunction = local_ZSTD_decodeLiteralsBlock; benchName = "ZSTD_decodeLiteralsBlock";
        break;
    case 32:
        benchFunction = local_ZSTD_decodeSeqHeaders; benchName = "ZSTD_decodeSeqHeaders";
        break;
#endif
	case 41:
        benchFunction = local_ZSTD_compressStream; benchName = "ZSTD_compressStream";
        break;
    case 42:
        benchFunction = local_ZSTD_decompressStream; benchName = "ZSTD_decompressStream";
        break;
    default :
        return 0;
    }

    /* Allocation */
    dstBuff = (BYTE*)malloc(dstBuffSize);
    buff2 = malloc(dstBuffSize);
    if ((!dstBuff) || (!buff2)) {
        DISPLAY("\nError: not enough memory!\n");
        free(dstBuff); free(buff2);
        return 12;
    }

    /* Preparation */
    switch(benchNb)
    {
    case 2:
        g_cSize = ZSTD_compress(buff2, dstBuffSize, src, srcSize, 1);
        break;
#ifndef ZSTD_DLL_IMPORT
    case 11 :
        if (g_zcc==NULL) g_zcc = ZSTD_createCCtx();
        break;
    case 12 :
        if (g_zcc==NULL) g_zcc = ZSTD_createCCtx();
        break;
    case 13 :
        if (g_zdc==NULL) g_zdc = ZSTD_createDCtx();
        g_cSize = ZSTD_compress(buff2, dstBuffSize, src, srcSize, 1);
        break;
    case 31:  /* ZSTD_decodeLiteralsBlock */
        if (g_zdc==NULL) g_zdc = ZSTD_createDCtx();
        {   blockProperties_t bp;
            ZSTD_frameParams zfp;
            size_t frameHeaderSize, skippedSize;
            g_cSize = ZSTD_compress(dstBuff, dstBuffSize, src, srcSize, 1);
            frameHeaderSize = ZSTD_getFrameParams(&zfp, dstBuff, ZSTD_frameHeaderSize_min);
            if (frameHeaderSize==0) frameHeaderSize = ZSTD_frameHeaderSize_min;
            ZSTD_getcBlockSize(dstBuff+frameHeaderSize, dstBuffSize, &bp);  /* Get 1st block type */
            if (bp.blockType != bt_compressed) {
                DISPLAY("ZSTD_decodeLiteralsBlock : impossible to test on this sample (not compressible)\n");
                goto _cleanOut;
            }
            skippedSize = frameHeaderSize + ZSTD_blockHeaderSize;
            memcpy(buff2, dstBuff+skippedSize, g_cSize-skippedSize);
            srcSize = srcSize > 128 KB ? 128 KB : srcSize;    /* speed relative to block */
            break;
        }
    case 32:   /* ZSTD_decodeSeqHeaders */
        if (g_zdc==NULL) g_zdc = ZSTD_createDCtx();
        {   blockProperties_t bp;
            ZSTD_frameParams zfp;
            const BYTE* ip = dstBuff;
            const BYTE* iend;
            size_t frameHeaderSize, cBlockSize;
            ZSTD_compress(dstBuff, dstBuffSize, src, srcSize, 1);   /* it would be better to use direct block compression here */
            g_cSize = ZSTD_compress(dstBuff, dstBuffSize, src, srcSize, 1);
            frameHeaderSize = ZSTD_getFrameParams(&zfp, dstBuff, ZSTD_frameHeaderSize_min);
            if (frameHeaderSize==0) frameHeaderSize = ZSTD_frameHeaderSize_min;
            ip += frameHeaderSize;   /* Skip frame Header */
            cBlockSize = ZSTD_getcBlockSize(ip, dstBuffSize, &bp);   /* Get 1st block type */
            if (bp.blockType != bt_compressed) {
                DISPLAY("ZSTD_decodeSeqHeaders : impossible to test on this sample (not compressible)\n");
                goto _cleanOut;
            }
            iend = ip + ZSTD_blockHeaderSize + cBlockSize;   /* End of first block */
            ip += ZSTD_blockHeaderSize;                      /* skip block header */
            ZSTD_decompressBegin(g_zdc);
            ip += ZSTD_decodeLiteralsBlock(g_zdc, ip, iend-ip);   /* skip literal segment */
            g_cSize = iend-ip;
            memcpy(buff2, ip, g_cSize);   /* copy rest of block (it starts by SeqHeader) */
            srcSize = srcSize > 128 KB ? 128 KB : srcSize;   /* speed relative to block */
            break;
        }
#else
    case 31:
        goto _cleanOut;
#endif
    case 41 :
        if (g_cstream==NULL) g_cstream = ZSTD_createCStream();
        break;
    case 42 :
        if (g_dstream==NULL) g_dstream = ZSTD_createDStream();
        g_cSize = ZSTD_compress(buff2, dstBuffSize, src, srcSize, 1);
        break;

    /* test functions */
    /* by convention, test functions can be added > 100 */

    default : ;
    }

    { size_t i; for (i=0; i<dstBuffSize; i++) dstBuff[i]=(BYTE)i; }     /* warming up memory */

    {   U32 loopNb;
        for (loopNb = 1; loopNb <= g_nbIterations; loopNb++) {
            clock_t const timeLoop = TIMELOOP_S * CLOCKS_PER_SEC;
            clock_t clockStart;
            U32 nbRounds;
            size_t benchResult=0;
            double averageTime;

            DISPLAY("%2i- %-30.30s : \r", loopNb, benchName);

            clockStart = clock();
            while (clock() == clockStart);
            clockStart = clock();
            for (nbRounds=0; BMK_clockSpan(clockStart) < timeLoop; nbRounds++) {
                benchResult = benchFunction(dstBuff, dstBuffSize, buff2, src, srcSize);
                if (ZSTD_isError(benchResult)) { DISPLAY("ERROR ! %s() => %s !! \n", benchName, ZSTD_getErrorName(benchResult)); exit(1); }
            }
            averageTime = (((double)BMK_clockSpan(clockStart)) / CLOCKS_PER_SEC) / nbRounds;
            if (averageTime < bestTime) bestTime = averageTime;
            DISPLAY("%2i- %-30.30s : %7.1f MB/s  (%9u)\r", loopNb, benchName, (double)srcSize / (1 MB) / bestTime, (U32)benchResult);
    }   }
    DISPLAY("%2u\n", benchNb);

_cleanOut:
    free(dstBuff);
    free(buff2);
    return 0;
}


static int benchSample(U32 benchNb)
{
    size_t const benchedSize = g_sampleSize;
    const char* name = "Sample 10MiB";

    /* Allocation */
    void* origBuff = malloc(benchedSize);
    if (!origBuff) { DISPLAY("\nError: not enough memory!\n"); return 12; }

    /* Fill buffer */
    RDG_genBuffer(origBuff, benchedSize, g_compressibility, 0.0, 0);

    /* bench */
    DISPLAY("\r%79s\r", "");
    DISPLAY(" %s : \n", name);
    if (benchNb)
        benchMem(origBuff, benchedSize, benchNb);
    else
        for (benchNb=0; benchNb<100; benchNb++) benchMem(origBuff, benchedSize, benchNb);

    free(origBuff);
    return 0;
}


static int benchFiles(const char** fileNamesTable, const int nbFiles, U32 benchNb)
{
    /* Loop for each file */
    int fileIdx;
    for (fileIdx=0; fileIdx<nbFiles; fileIdx++) {
        const char* inFileName = fileNamesTable[fileIdx];
        FILE* inFile = fopen( inFileName, "rb" );
        U64   inFileSize;
        size_t benchedSize;
        void* origBuff;

        /* Check file existence */
        if (inFile==NULL) { DISPLAY( "Pb opening %s\n", inFileName); return 11; }

        /* Memory allocation & restrictions */
        inFileSize = UTIL_getFileSize(inFileName);
        benchedSize = BMK_findMaxMem(inFileSize*3) / 3;
        if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
        if (benchedSize < inFileSize)
            DISPLAY("Not enough memory for '%s' full size; testing %u MB only...\n", inFileName, (U32)(benchedSize>>20));

        /* Alloc */
        origBuff = malloc(benchedSize);
        if (!origBuff) { DISPLAY("\nError: not enough memory!\n"); fclose(inFile); return 12; }

        /* Fill input buffer */
        DISPLAY("Loading %s...       \r", inFileName);
        {
            size_t readSize = fread(origBuff, 1, benchedSize, inFile);
            fclose(inFile);
            if (readSize != benchedSize) {
                DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
                free(origBuff);
                return 13;
        }   }

        /* bench */
        DISPLAY("\r%79s\r", "");
        DISPLAY(" %s : \n", inFileName);
        if (benchNb)
            benchMem(origBuff, benchedSize, benchNb);
        else
            for (benchNb=0; benchNb<100; benchNb++) benchMem(origBuff, benchedSize, benchNb);

        free(origBuff);
    }

    return 0;
}


static int usage(const char* exename)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [arg] file1 file2 ... fileX\n", exename);
    DISPLAY( "Arguments :\n");
    DISPLAY( " -H/-h  : Help (this text + advanced options)\n");
    return 0;
}

static int usage_advanced(const char* exename)
{
    usage(exename);
    DISPLAY( "\nAdvanced options :\n");
    DISPLAY( " -b#    : test only function # \n");
    DISPLAY( " -i#    : iteration loops [1-9](default : %i)\n", NBLOOPS);
    DISPLAY( " -P#    : sample compressibility (default : %.1f%%)\n", COMPRESSIBILITY_DEFAULT * 100);
    return 0;
}

static int badusage(const char* exename)
{
    DISPLAY("Wrong parameters\n");
    usage(exename);
    return 1;
}

int main(int argc, const char** argv)
{
    int i, filenamesStart=0, result;
    const char* exename = argv[0];
    const char* input_filename = NULL;
    U32 benchNb = 0, main_pause = 0;

    DISPLAY(WELCOME_MESSAGE);
    if (argc<1) return badusage(exename);

    for(i=1; i<argc; i++) {
        const char* argument = argv[i];
        if(!argument) continue;   /* Protection if argument empty */

        /* Commands (note : aggregated commands are allowed) */
        if (argument[0]=='-') {

            while (argument[1]!=0) {
                argument++;

                switch(argument[0])
                {
                    /* Display help on usage */
                case 'h':
                case 'H': return usage_advanced(exename);

                    /* Pause at the end (hidden option) */
                case 'p': main_pause = 1; break;

                    /* Select specific algorithm to bench */
                case 'b':
                    benchNb = 0;
                    while ((argument[1]>= '0') && (argument[1]<= '9')) {
                        benchNb *= 10;
                        benchNb += argument[1] - '0';
                        argument++;
                    }
                    break;

                    /* Modify Nb Iterations */
                case 'i':
                    if ((argument[1] >='0') && (argument[1] <='9')) {
                        int iters = argument[1] - '0';
                        BMK_SetNbIterations(iters);
                        argument++;
                    }
                    break;

                    /* Select compressibility of synthetic sample */
                case 'P':
                    {   U32 proba32 = 0;
                        while ((argument[1]>= '0') && (argument[1]<= '9')) {
                            proba32 *= 10;
                            proba32 += argument[1] - '0';
                            argument++;
                        }
                        g_compressibility = (double)proba32 / 100.;
                    }
                    break;

                    /* Unknown command */
                default : return badusage(exename);
                }
            }
            continue;
        }

        /* first provided filename is input */
        if (!input_filename) { input_filename=argument; filenamesStart=i; continue; }
    }

    if (filenamesStart==0)   /* no input file */
        result = benchSample(benchNb);
    else
        result = benchFiles(argv+filenamesStart, argc-filenamesStart, benchNb);

    if (main_pause) { int unused; printf("press enter...\n"); unused = getchar(); (void)unused; }

    return result;
}
