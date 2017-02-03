/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


/*-************************************
*  Tuning parameters
**************************************/
#ifndef ZSTDCLI_CLEVEL_DEFAULT
#  define ZSTDCLI_CLEVEL_DEFAULT 3
#endif

#ifndef ZSTDCLI_CLEVEL_MAX
#  define ZSTDCLI_CLEVEL_MAX 19   /* when not using --ultra */
#endif



/*-************************************
*  Dependencies
**************************************/
#include "platform.h" /* IS_CONSOLE, PLATFORM_POSIX_VERSION */
#include "util.h"     /* UTIL_HAS_CREATEFILELIST, UTIL_createFileList */
#include <string.h>   /* strcmp, strlen */
#include <errno.h>    /* errno */
#include "fileio.h"
#ifndef ZSTD_NOBENCH
#  include "bench.h"  /* BMK_benchFiles, BMK_SetNbSeconds */
#endif
#ifndef ZSTD_NODICT
#  include "dibio.h"
#endif
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_maxCLevel */
#include "zstd.h"     /* ZSTD_VERSION_STRING */


/*-************************************
*  Constants
**************************************/
#define COMPRESSOR_NAME "zstd command line interface"
#ifndef ZSTD_VERSION
#  define ZSTD_VERSION "v" ZSTD_VERSION_STRING
#endif
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %i-bits %s, by %s ***\n", COMPRESSOR_NAME, (int)(sizeof(size_t)*8), ZSTD_VERSION, AUTHOR

#define ZSTD_EXTENSION ".zst"
#define ZSTD_CAT "zstdcat"
#define ZSTD_UNZSTD "unzstd"

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define DEFAULT_DISPLAY_LEVEL 2

static const char*    g_defaultDictName = "dictionary";
static const unsigned g_defaultMaxDictSize = 110 KB;
static const int      g_defaultDictCLevel = 3;
static const unsigned g_defaultSelectivityLevel = 9;
#define OVERLAP_LOG_DEFAULT 9999
static U32 g_overlapLog = OVERLAP_LOG_DEFAULT;


/*-************************************
*  Display Macros
**************************************/
#define DISPLAY(...)           fprintf(displayOut, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...)   if (displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static FILE* displayOut;
static unsigned displayLevel = DEFAULT_DISPLAY_LEVEL;   /* 0 : no display,  1: errors,  2 : + result + interaction + warnings,  3 : + progression,  4 : + information */


/*-************************************
*  Command Line
**************************************/
static int usage(const char* programName)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [args] [FILE(s)] [-o file]\n", programName);
    DISPLAY( "\n");
    DISPLAY( "FILE    : a filename\n");
    DISPLAY( "          with no FILE, or when FILE is - , read standard input\n");
    DISPLAY( "Arguments :\n");
#ifndef ZSTD_NOCOMPRESS
    DISPLAY( " -#     : # compression level (1-%d, default:%d) \n", ZSTDCLI_CLEVEL_MAX, ZSTDCLI_CLEVEL_DEFAULT);
#endif
#ifndef ZSTD_NODECOMPRESS
    DISPLAY( " -d     : decompression \n");
#endif
    DISPLAY( " -D file: use `file` as Dictionary \n");
    DISPLAY( " -o file: result stored into `file` (only if 1 input file) \n");
    DISPLAY( " -f     : overwrite output without prompting \n");
    DISPLAY( "--rm    : remove source file(s) after successful de/compression \n");
    DISPLAY( " -k     : preserve source file(s) (default) \n");
    DISPLAY( " -h/-H  : display help/long help and exit\n");
    return 0;
}

static int usage_advanced(const char* programName)
{
    DISPLAY(WELCOME_MESSAGE);
    usage(programName);
    DISPLAY( "\n");
    DISPLAY( "Advanced arguments :\n");
    DISPLAY( " -V     : display Version number and exit\n");
    DISPLAY( " -v     : verbose mode; specify multiple times to increase log level (default:%d)\n", DEFAULT_DISPLAY_LEVEL);
    DISPLAY( " -q     : suppress warnings; specify twice to suppress errors too\n");
    DISPLAY( " -c     : force write to standard output, even if it is the console\n");
#ifdef UTIL_HAS_CREATEFILELIST
    DISPLAY( " -r     : operate recursively on directories \n");
#endif
#ifndef ZSTD_NOCOMPRESS
    DISPLAY( "--ultra : enable levels beyond %i, up to %i (requires more memory)\n", ZSTDCLI_CLEVEL_MAX, ZSTD_maxCLevel());
    DISPLAY( "--no-dictID : don't write dictID into header (dictionary compression)\n");
    DISPLAY( "--[no-]check : integrity check (default:enabled) \n");
#ifdef ZSTD_MULTITHREAD
    DISPLAY( " -T#    : use # threads for compression (default:1) \n");
    DISPLAY( " -B#    : select size of independent sections (default:0==automatic) \n");
#endif
#endif
#ifndef ZSTD_NODECOMPRESS
    DISPLAY( "--test  : test compressed file integrity \n");
    DISPLAY( "--[no-]sparse : sparse mode (default:enabled on file, disabled on stdout)\n");
#endif
    DISPLAY( " -M#    : Set a memory usage limit for decompression \n");
    DISPLAY( "--      : All arguments after \"--\" are treated as files \n");
#ifndef ZSTD_NODICT
    DISPLAY( "\n");
    DISPLAY( "Dictionary builder :\n");
    DISPLAY( "--train ## : create a dictionary from a training set of files \n");
    DISPLAY( "--cover=k=#,d=# : use the cover algorithm with parameters k and d \n");
    DISPLAY( "--optimize-cover[=steps=#,k=#,d=#] : optimize cover parameters with optional parameters\n");
    DISPLAY( " -o file : `file` is dictionary name (default: %s) \n", g_defaultDictName);
    DISPLAY( "--maxdict ## : limit dictionary to specified size (default : %u) \n", g_defaultMaxDictSize);
    DISPLAY( " -s#    : dictionary selectivity level (default: %u)\n", g_defaultSelectivityLevel);
    DISPLAY( "--dictID ## : force dictionary ID to specified value (default: random)\n");
#endif
#ifndef ZSTD_NOBENCH
    DISPLAY( "\n");
    DISPLAY( "Benchmark arguments :\n");
    DISPLAY( " -b#    : benchmark file(s), using # compression level (default : 1) \n");
    DISPLAY( " -e#    : test all compression levels from -bX to # (default: 1)\n");
    DISPLAY( " -i#    : minimum evaluation time in seconds (default : 3s)\n");
    DISPLAY( " -B#    : cut file into independent blocks of size # (default: no block)\n");
#endif
    return 0;
}

static int badusage(const char* programName)
{
    DISPLAYLEVEL(1, "Incorrect parameters\n");
    if (displayLevel >= 1) usage(programName);
    return 1;
}

static void waitEnter(void)
{
    int unused;
    DISPLAY("Press enter to continue...\n");
    unused = getchar();
    (void)unused;
}

/*! readU32FromChar() :
    @return : unsigned integer value read from input in `char` format
    allows and interprets K, KB, KiB, M, MB and MiB suffix.
    Will also modify `*stringPtr`, advancing it to position where it stopped reading.
    Note : function result can overflow if digit string > MAX_UINT */
static unsigned readU32FromChar(const char** stringPtr)
{
    unsigned result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9'))
        result *= 10, result += **stringPtr - '0', (*stringPtr)++ ;
    if ((**stringPtr=='K') || (**stringPtr=='M')) {
        result <<= 10;
        if (**stringPtr=='M') result <<= 10;
        (*stringPtr)++ ;
        if (**stringPtr=='i') (*stringPtr)++;
        if (**stringPtr=='B') (*stringPtr)++;
    }
    return result;
}

/** longCommandWArg() :
 *  check if *stringPtr is the same as longCommand.
 *  If yes, @return 1 and advances *stringPtr to the position which immediately follows longCommand.
 *  @return 0 and doesn't modify *stringPtr otherwise.
 */
static unsigned longCommandWArg(const char** stringPtr, const char* longCommand)
{
    size_t const comSize = strlen(longCommand);
    int const result = !strncmp(*stringPtr, longCommand, comSize);
    if (result) *stringPtr += comSize;
    return result;
}


#ifndef ZSTD_NODICT
/**
 * parseCoverParameters() :
 * reads cover parameters from *stringPtr (e.g. "--cover=smoothing=100,kmin=48,kstep=4,kmax=64,d=8") into *params
 * @return 1 means that cover parameters were correct
 * @return 0 in case of malformed parameters
 */
static unsigned parseCoverParameters(const char* stringPtr, COVER_params_t *params)
{
    memset(params, 0, sizeof(*params));
    for (; ;) {
        if (longCommandWArg(&stringPtr, "k=")) { params->k = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        if (longCommandWArg(&stringPtr, "d=")) { params->d = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        if (longCommandWArg(&stringPtr, "steps=")) { params->steps = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        return 0;
    }
    if (stringPtr[0] != 0) return 0;
    DISPLAYLEVEL(4, "k=%u\nd=%u\nsteps=%u\n", params->k, params->d, params->steps);
    return 1;
}
#endif


/** parseCompressionParameters() :
 *  reads compression parameters from *stringPtr (e.g. "--zstd=wlog=23,clog=23,hlog=22,slog=6,slen=3,tlen=48,strat=6") into *params
 *  @return 1 means that compression parameters were correct
 *  @return 0 in case of malformed parameters
 */
static unsigned parseCompressionParameters(const char* stringPtr, ZSTD_compressionParameters* params)
{
    for ( ; ;) {
        if (longCommandWArg(&stringPtr, "windowLog=") || longCommandWArg(&stringPtr, "wlog=")) { params->windowLog = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        if (longCommandWArg(&stringPtr, "chainLog=") || longCommandWArg(&stringPtr, "clog=")) { params->chainLog = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        if (longCommandWArg(&stringPtr, "hashLog=") || longCommandWArg(&stringPtr, "hlog=")) { params->hashLog = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        if (longCommandWArg(&stringPtr, "searchLog=") || longCommandWArg(&stringPtr, "slog=")) { params->searchLog = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        if (longCommandWArg(&stringPtr, "searchLength=") || longCommandWArg(&stringPtr, "slen=")) { params->searchLength = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        if (longCommandWArg(&stringPtr, "targetLength=") || longCommandWArg(&stringPtr, "tlen=")) { params->targetLength = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        if (longCommandWArg(&stringPtr, "strategy=") || longCommandWArg(&stringPtr, "strat=")) { params->strategy = (ZSTD_strategy)(1 + readU32FromChar(&stringPtr)); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        if (longCommandWArg(&stringPtr, "overlapLog=") || longCommandWArg(&stringPtr, "ovlog=")) { g_overlapLog = readU32FromChar(&stringPtr); if (stringPtr[0]==',') { stringPtr++; continue; } else break; }
        return 0;
    }

    if (stringPtr[0] != 0) return 0; /* check the end of string */
    DISPLAYLEVEL(4, "windowLog=%d\nchainLog=%d\nhashLog=%d\nsearchLog=%d\n", params->windowLog, params->chainLog, params->hashLog, params->searchLog);
    DISPLAYLEVEL(4, "searchLength=%d\ntargetLength=%d\nstrategy=%d\n", params->searchLength, params->targetLength, params->strategy);
    return 1;
}


typedef enum { zom_compress, zom_decompress, zom_test, zom_bench, zom_train } zstd_operation_mode;

#define CLEAN_RETURN(i) { operationResult = (i); goto _end; }

int main(int argCount, const char* argv[])
{
    int argNb,
        forceStdout=0,
        main_pause=0,
        nextEntryIsDictionary=0,
        operationResult=0,
        nextArgumentIsOutFileName=0,
        nextArgumentIsMaxDict=0,
        nextArgumentIsDictID=0,
        nextArgumentsAreFiles=0,
        ultra=0,
        lastCommand = 0,
        nbThreads = 1;
    unsigned bench_nbSeconds = 3;   /* would be better if this value was synchronized from bench */
    size_t blockSize = 0;
    zstd_operation_mode operation = zom_compress;
    ZSTD_compressionParameters compressionParams;
    int cLevel = ZSTDCLI_CLEVEL_DEFAULT;
    int cLevelLast = 1;
    unsigned recursive = 0;
    unsigned memLimit = 0;
    const char** filenameTable = (const char**)malloc(argCount * sizeof(const char*));   /* argCount >= 1 */
    unsigned filenameIdx = 0;
    const char* programName = argv[0];
    const char* outFileName = NULL;
    const char* dictFileName = NULL;
    unsigned maxDictSize = g_defaultMaxDictSize;
    unsigned dictID = 0;
    int dictCLevel = g_defaultDictCLevel;
    unsigned dictSelect = g_defaultSelectivityLevel;
#ifdef UTIL_HAS_CREATEFILELIST
    const char** extendedFileList = NULL;
    char* fileNamesBuf = NULL;
    unsigned fileNamesNb;
#endif
#ifndef ZSTD_NODICT
    COVER_params_t coverParams;
    int cover = 0;
#endif

    /* init */
    (void)recursive; (void)cLevelLast;    /* not used when ZSTD_NOBENCH set */
    (void)dictCLevel; (void)dictSelect; (void)dictID;  (void)maxDictSize; /* not used when ZSTD_NODICT set */
    (void)ultra; (void)cLevel; /* not used when ZSTD_NOCOMPRESS set */
    (void)memLimit;   /* not used when ZSTD_NODECOMPRESS set */
    if (filenameTable==NULL) { DISPLAY("zstd: %s \n", strerror(errno)); exit(1); }
    filenameTable[0] = stdinmark;
    displayOut = stderr;
    /* Pick out program name from path. Don't rely on stdlib because of conflicting behavior */
    {   size_t pos;
        for (pos = (int)strlen(programName); pos > 0; pos--) { if (programName[pos] == '/') { pos++; break; } }
        programName += pos;
    }

    /* preset behaviors */
    if (!strcmp(programName, ZSTD_UNZSTD)) operation=zom_decompress;
    if (!strcmp(programName, ZSTD_CAT)) { operation=zom_decompress; forceStdout=1; FIO_overwriteMode(); outFileName=stdoutmark; displayLevel=1; }
    memset(&compressionParams, 0, sizeof(compressionParams));

    /* command switches */
    for (argNb=1; argNb<argCount; argNb++) {
        const char* argument = argv[argNb];
        if(!argument) continue;   /* Protection if argument empty */

        if (nextArgumentsAreFiles==0) {
            /* "-" means stdin/stdout */
            if (!strcmp(argument, "-")){
                if (!filenameIdx) {
                    filenameIdx=1, filenameTable[0]=stdinmark;
                    outFileName=stdoutmark;
                    displayLevel-=(displayLevel==2);
                    continue;
            }   }

            /* Decode commands (note : aggregated commands are allowed) */
            if (argument[0]=='-') {

                if (argument[1]=='-') {
                    /* long commands (--long-word) */
                    if (!strcmp(argument, "--")) { nextArgumentsAreFiles=1; continue; }   /* only file names allowed from now on */
                    if (!strcmp(argument, "--compress")) { operation=zom_compress; continue; }
                    if (!strcmp(argument, "--decompress")) { operation=zom_decompress; continue; }
                    if (!strcmp(argument, "--uncompress")) { operation=zom_decompress; continue; }
                    if (!strcmp(argument, "--force")) { FIO_overwriteMode(); continue; }
                    if (!strcmp(argument, "--version")) { displayOut=stdout; DISPLAY(WELCOME_MESSAGE); CLEAN_RETURN(0); }
                    if (!strcmp(argument, "--help")) { displayOut=stdout; CLEAN_RETURN(usage_advanced(programName)); }
                    if (!strcmp(argument, "--verbose")) { displayLevel++; continue; }
                    if (!strcmp(argument, "--quiet")) { displayLevel--; continue; }
                    if (!strcmp(argument, "--stdout")) { forceStdout=1; outFileName=stdoutmark; displayLevel-=(displayLevel==2); continue; }
                    if (!strcmp(argument, "--ultra")) { ultra=1; continue; }
                    if (!strcmp(argument, "--check")) { FIO_setChecksumFlag(2); continue; }
                    if (!strcmp(argument, "--no-check")) { FIO_setChecksumFlag(0); continue; }
                    if (!strcmp(argument, "--sparse")) { FIO_setSparseWrite(2); continue; }
                    if (!strcmp(argument, "--no-sparse")) { FIO_setSparseWrite(0); continue; }
                    if (!strcmp(argument, "--test")) { operation=zom_test; continue; }
                    if (!strcmp(argument, "--train")) { operation=zom_train; outFileName=g_defaultDictName; continue; }
                    if (!strcmp(argument, "--maxdict")) { nextArgumentIsMaxDict=1; lastCommand=1; continue; }
                    if (!strcmp(argument, "--dictID")) { nextArgumentIsDictID=1; lastCommand=1; continue; }
                    if (!strcmp(argument, "--no-dictID")) { FIO_setDictIDFlag(0); continue; }
                    if (!strcmp(argument, "--keep")) { FIO_setRemoveSrcFile(0); continue; }
                    if (!strcmp(argument, "--rm")) { FIO_setRemoveSrcFile(1); continue; }

                    /* long commands with arguments */
#ifndef  ZSTD_NODICT
                    if (longCommandWArg(&argument, "--cover=")) {
                      cover=1; if (!parseCoverParameters(argument, &coverParams)) CLEAN_RETURN(badusage(programName));
                      continue;
                    }
                    if (longCommandWArg(&argument, "--optimize-cover")) {
                      cover=2;
                      /* Allow optional arguments following an = */
                      if (*argument == 0) { memset(&coverParams, 0, sizeof(coverParams)); }
                      else if (*argument++ != '=') { CLEAN_RETURN(badusage(programName)); }
                      else if (!parseCoverParameters(argument, &coverParams)) { CLEAN_RETURN(badusage(programName)); }
                      continue;
                    }
#endif
                    if (longCommandWArg(&argument, "--memlimit=")) { memLimit = readU32FromChar(&argument); continue; }
                    if (longCommandWArg(&argument, "--memory=")) { memLimit = readU32FromChar(&argument); continue; }
                    if (longCommandWArg(&argument, "--memlimit-decompress=")) { memLimit = readU32FromChar(&argument); continue; }
                    if (longCommandWArg(&argument, "--block-size=")) { blockSize = readU32FromChar(&argument); continue; }
                    if (longCommandWArg(&argument, "--zstd=")) { if (!parseCompressionParameters(argument, &compressionParams)) CLEAN_RETURN(badusage(programName)); continue; }
                    /* fall-through, will trigger bad_usage() later on */
                }

                argument++;
                while (argument[0]!=0) {
                    if (lastCommand) {
                        DISPLAY("error : command must be followed by argument \n");
                        return 1;
                    }
#ifndef ZSTD_NOCOMPRESS
                    /* compression Level */
                    if ((*argument>='0') && (*argument<='9')) {
                        dictCLevel = cLevel = readU32FromChar(&argument);
                        continue;
                    }
#endif

                    switch(argument[0])
                    {
                        /* Display help */
                    case 'V': displayOut=stdout; DISPLAY(WELCOME_MESSAGE); CLEAN_RETURN(0);   /* Version Only */
                    case 'H':
                    case 'h': displayOut=stdout; CLEAN_RETURN(usage_advanced(programName));

                         /* Compress */
                    case 'z': operation=zom_compress; argument++; break;

                         /* Decoding */
                    case 'd':
#ifndef ZSTD_NOBENCH
                            if (operation==zom_bench) { BMK_setDecodeOnlyMode(1); argument++; break; }  /* benchmark decode (hidden option) */
#endif
                            operation=zom_decompress; argument++; break;

                        /* Force stdout, even if stdout==console */
                    case 'c': forceStdout=1; outFileName=stdoutmark; argument++; break;

                        /* Use file content as dictionary */
                    case 'D': nextEntryIsDictionary = 1; lastCommand = 1; argument++; break;

                        /* Overwrite */
                    case 'f': FIO_overwriteMode(); forceStdout=1; argument++; break;

                        /* Verbose mode */
                    case 'v': displayLevel++; argument++; break;

                        /* Quiet mode */
                    case 'q': displayLevel--; argument++; break;

                        /* keep source file (default); for gzip/xz compatibility */
                    case 'k': FIO_setRemoveSrcFile(0); argument++; break;

                        /* Checksum */
                    case 'C': argument++; FIO_setChecksumFlag(2); break;

                        /* test compressed file */
                    case 't': operation=zom_test; argument++; break;

                        /* destination file name */
                    case 'o': nextArgumentIsOutFileName=1; lastCommand=1; argument++; break;

                        /* limit decompression memory */
                    case 'M':
                        argument++;
                        memLimit = readU32FromChar(&argument);
                        break;

#ifdef UTIL_HAS_CREATEFILELIST
                        /* recursive */
                    case 'r': recursive=1; argument++; break;
#endif

#ifndef ZSTD_NOBENCH
                        /* Benchmark */
                    case 'b':
                        operation=zom_bench;
                        argument++;
                        break;

                        /* range bench (benchmark only) */
                    case 'e':
                        /* compression Level */
                        argument++;
                        cLevelLast = readU32FromChar(&argument);
                        break;

                        /* Modify Nb Iterations (benchmark only) */
                    case 'i':
                        argument++;
                        bench_nbSeconds = readU32FromChar(&argument);
                        break;

                        /* cut input into blocks (benchmark only) */
                    case 'B':
                        argument++;
                        blockSize = readU32FromChar(&argument);
                        break;

#endif   /* ZSTD_NOBENCH */

                        /* nb of threads (hidden option) */
                    case 'T':
                        argument++;
                        nbThreads = readU32FromChar(&argument);
                        break;

                        /* Dictionary Selection level */
                    case 's':
                        argument++;
                        dictSelect = readU32FromChar(&argument);
                        break;

                        /* Pause at the end (-p) or set an additional param (-p#) (hidden option) */
                    case 'p': argument++;
#ifndef ZSTD_NOBENCH
                        if ((*argument>='0') && (*argument<='9')) {
                            BMK_setAdditionalParam(readU32FromChar(&argument));
                        } else
#endif
                            main_pause=1;
                        break;
                        /* unknown command */
                    default : CLEAN_RETURN(badusage(programName));
                    }
                }
                continue;
            }   /* if (argument[0]=='-') */

            if (nextArgumentIsMaxDict) {
                nextArgumentIsMaxDict = 0;
                lastCommand = 0;
                maxDictSize = readU32FromChar(&argument);
                continue;
            }

            if (nextArgumentIsDictID) {
                nextArgumentIsDictID = 0;
                lastCommand = 0;
                dictID = readU32FromChar(&argument);
                continue;
            }

        }   /* if (nextArgumentIsAFile==0) */

        if (nextEntryIsDictionary) {
            nextEntryIsDictionary = 0;
            lastCommand = 0;
            dictFileName = argument;
            continue;
        }

        if (nextArgumentIsOutFileName) {
            nextArgumentIsOutFileName = 0;
            lastCommand = 0;
            outFileName = argument;
            if (!strcmp(outFileName, "-")) outFileName = stdoutmark;
            continue;
        }

        /* add filename to list */
        filenameTable[filenameIdx++] = argument;
    }

    if (lastCommand) { DISPLAY("error : command must be followed by argument \n"); return 1; }  /* forgotten argument */

    /* Welcome message (if verbose) */
    DISPLAYLEVEL(3, WELCOME_MESSAGE);
#ifdef _POSIX_C_SOURCE
    DISPLAYLEVEL(4, "_POSIX_C_SOURCE defined: %ldL\n", (long) _POSIX_C_SOURCE);
#endif
#ifdef _POSIX_VERSION
    DISPLAYLEVEL(4, "_POSIX_VERSION defined: %ldL\n", (long) _POSIX_VERSION);
#endif
#ifdef PLATFORM_POSIX_VERSION
    DISPLAYLEVEL(4, "PLATFORM_POSIX_VERSION defined: %ldL\n", (long) PLATFORM_POSIX_VERSION);
#endif

#ifdef UTIL_HAS_CREATEFILELIST
    if (recursive) {  /* at this stage, filenameTable is a list of paths, which can contain both files and directories */
        extendedFileList = UTIL_createFileList(filenameTable, filenameIdx, &fileNamesBuf, &fileNamesNb);
        if (extendedFileList) {
            unsigned u;
            for (u=0; u<fileNamesNb; u++) DISPLAYLEVEL(4, "%u %s\n", u, extendedFileList[u]);
            free((void*)filenameTable);
            filenameTable = extendedFileList;
            filenameIdx = fileNamesNb;
        }
    }
#endif

    /* Check if benchmark is selected */
    if (operation==zom_bench) {
#ifndef ZSTD_NOBENCH
        BMK_setNotificationLevel(displayLevel);
        BMK_setBlockSize(blockSize);
        BMK_setNbThreads(nbThreads);
        BMK_setNbSeconds(bench_nbSeconds);
        BMK_benchFiles(filenameTable, filenameIdx, dictFileName, cLevel, cLevelLast, &compressionParams);
#endif
        (void)bench_nbSeconds;
        goto _end;
    }

    /* Check if dictionary builder is selected */
    if (operation==zom_train) {
#ifndef ZSTD_NODICT
        if (cover) {
            coverParams.nbThreads = nbThreads;
            coverParams.compressionLevel = dictCLevel;
            coverParams.notificationLevel = displayLevel;
            coverParams.dictID = dictID;
            DiB_trainFromFiles(outFileName, maxDictSize, filenameTable, filenameIdx, NULL, &coverParams, cover - 1);
        } else {
            ZDICT_params_t dictParams;
            memset(&dictParams, 0, sizeof(dictParams));
            dictParams.compressionLevel = dictCLevel;
            dictParams.selectivityLevel = dictSelect;
            dictParams.notificationLevel = displayLevel;
            dictParams.dictID = dictID;
            DiB_trainFromFiles(outFileName, maxDictSize, filenameTable, filenameIdx, &dictParams, NULL, 0);
        }
#endif
        goto _end;
    }

    /* No input filename ==> use stdin and stdout */
    filenameIdx += !filenameIdx;   /* filenameTable[0] is stdin by default */
    if (!strcmp(filenameTable[0], stdinmark) && !outFileName) outFileName = stdoutmark;   /* when input is stdin, default output is stdout */

    /* Check if input/output defined as console; trigger an error in this case */
    if (!strcmp(filenameTable[0], stdinmark) && IS_CONSOLE(stdin) ) CLEAN_RETURN(badusage(programName));
    if (outFileName && !strcmp(outFileName, stdoutmark) && IS_CONSOLE(stdout) && strcmp(filenameTable[0], stdinmark) && !(forceStdout && (operation==zom_decompress)))
        CLEAN_RETURN(badusage(programName));

    /* user-selected output filename, only possible with a single file */
    if (outFileName && strcmp(outFileName,stdoutmark) && strcmp(outFileName,nulmark) && (filenameIdx>1)) {
        DISPLAY("Too many files (%u) on the command line. \n", filenameIdx);
        CLEAN_RETURN(filenameIdx);
    }

#ifndef ZSTD_NOCOMPRESS
    /* check compression level limits */
    {   int const maxCLevel = ultra ? ZSTD_maxCLevel() : ZSTDCLI_CLEVEL_MAX;
        if (cLevel > maxCLevel) {
            DISPLAYLEVEL(2, "Warning : compression level higher than max, reduced to %i \n", maxCLevel);
            cLevel = maxCLevel;
    }   }
#endif

    /* No status message in pipe mode (stdin - stdout) or multi-files mode */
    if (!strcmp(filenameTable[0], stdinmark) && outFileName && !strcmp(outFileName,stdoutmark) && (displayLevel==2)) displayLevel=1;
    if ((filenameIdx>1) & (displayLevel==2)) displayLevel=1;

    /* IO Stream/File */
    FIO_setNotificationLevel(displayLevel);
    if (operation==zom_compress) {
#ifndef ZSTD_NOCOMPRESS
        FIO_setNbThreads(nbThreads);
        FIO_setBlockSize((U32)blockSize);
        if (g_overlapLog!=OVERLAP_LOG_DEFAULT) FIO_setOverlapLog(g_overlapLog);
        if ((filenameIdx==1) && outFileName)
          operationResult = FIO_compressFilename(outFileName, filenameTable[0], dictFileName, cLevel, &compressionParams);
        else
          operationResult = FIO_compressMultipleFilenames(filenameTable, filenameIdx, outFileName ? outFileName : ZSTD_EXTENSION, dictFileName, cLevel, &compressionParams);
#else
        DISPLAY("Compression not supported\n");
#endif
    } else {  /* decompression or test */
#ifndef ZSTD_NODECOMPRESS
        if (operation==zom_test) { outFileName=nulmark; FIO_setRemoveSrcFile(0); } /* test mode */
        FIO_setMemLimit(memLimit);
        if (filenameIdx==1 && outFileName)
            operationResult = FIO_decompressFilename(outFileName, filenameTable[0], dictFileName);
        else
            operationResult = FIO_decompressMultipleFilenames(filenameTable, filenameIdx, outFileName ? outFileName : ZSTD_EXTENSION, dictFileName);
#else
        DISPLAY("Decompression not supported\n");
#endif
    }

_end:
    if (main_pause) waitEnter();
#ifdef UTIL_HAS_CREATEFILELIST
    if (extendedFileList)
        UTIL_freeFileList(extendedFileList, fileNamesBuf);
    else
#endif
        free((void*)filenameTable);
    return operationResult;
}
