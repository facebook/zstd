/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


/*
  Note : this is a user program, not part of libzstd.
  The license of libzstd is BSD.
  The license of this command line program is GPLv2.
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
*  Includes
**************************************/
#include "util.h"     /* Compiler options, UTIL_HAS_CREATEFILELIST */
#include <string.h>   /* strcmp, strlen */
#include <ctype.h>    /* toupper */
#include <errno.h>    /* errno */
#include "fileio.h"
#ifndef ZSTD_NOBENCH
#  include "bench.h"  /* BMK_benchFiles, BMK_SetNbIterations */
#endif
#ifndef ZSTD_NODICT
#  include "dibio.h"
#endif
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_maxCLevel */
#include "zstd.h"     /* ZSTD_VERSION_STRING */


/*-************************************
*  OS-specific Includes
**************************************/
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <io.h>       /* _isatty */
#  define IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#elif defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE) || (defined(__APPLE__) && defined(__MACH__))  /* https://sourceforge.net/p/predef/wiki/OperatingSystems/ */
#  include <unistd.h>   /* isatty */
#  define IS_CONSOLE(stdStream) isatty(fileno(stdStream))
#else
#  define IS_CONSOLE(stdStream) 0
#endif


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
    DISPLAY( " -r     : operate recursively on directories\n");
#endif
#ifndef ZSTD_NOCOMPRESS
    DISPLAY( "--ultra : enable levels beyond %i, up to %i (requires more memory)\n", ZSTDCLI_CLEVEL_MAX, ZSTD_maxCLevel());
    DISPLAY( "--no-dictID : don't write dictID into header (dictionary compression)\n");
    DISPLAY( "--[no-]check : integrity check (default:enabled)\n");
#endif
#ifndef ZSTD_NODECOMPRESS
    DISPLAY( "--test  : test compressed file integrity \n");
    DISPLAY( "--[no-]sparse : sparse mode (default:enabled on file, disabled on stdout)\n");
#endif
    DISPLAY( "--      : All arguments after \"--\" are treated as files \n");
#ifndef ZSTD_NODICT
    DISPLAY( "\n");
    DISPLAY( "Dictionary builder :\n");
    DISPLAY( "--train ## : create a dictionary from a training set of files \n");
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
    @return : unsigned integer value reach from input in `char` format
    Will also modify `*stringPtr`, advancing it to position where it stopped reading.
    Note : this function can overflow if digit string > MAX_UINT */
static unsigned readU32FromChar(const char** stringPtr)
{
    unsigned result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9'))
        result *= 10, result += **stringPtr - '0', (*stringPtr)++ ;
    return result;
}


#define CLEAN_RETURN(i) { operationResult = (i); goto _end; }

int main(int argCount, const char* argv[])
{
    int argNb,
        bench=0,
        decode=0,
        testmode=0,
        forceStdout=0,
        main_pause=0,
        nextEntryIsDictionary=0,
        operationResult=0,
        dictBuild=0,
        nextArgumentIsOutFileName=0,
        nextArgumentIsMaxDict=0,
        nextArgumentIsDictID=0,
        nextArgumentIsFile=0,
        ultra=0,
        lastCommand = 0;
    int cLevel = ZSTDCLI_CLEVEL_DEFAULT;
    int cLevelLast = 1;
    unsigned recursive = 0;
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

    /* init */
    (void)recursive; (void)cLevelLast;    /* not used when ZSTD_NOBENCH set */
    (void)dictCLevel; (void)dictSelect; (void)dictID;  /* not used when ZSTD_NODICT set */
    (void)decode; (void)cLevel; (void)testmode;/* not used when ZSTD_NOCOMPRESS set */
    (void)ultra; /* not used when ZSTD_NODECOMPRESS set */
    if (filenameTable==NULL) { DISPLAY("zstd: %s \n", strerror(errno)); exit(1); }
    filenameTable[0] = stdinmark;
    displayOut = stderr;
    /* Pick out program name from path. Don't rely on stdlib because of conflicting behavior */
    {   size_t pos;
        for (pos = (int)strlen(programName); pos > 0; pos--) { if (programName[pos] == '/') { pos++; break; } }
        programName += pos;
    }

    /* preset behaviors */
    if (!strcmp(programName, ZSTD_UNZSTD)) decode=1;
    if (!strcmp(programName, ZSTD_CAT)) { decode=1; forceStdout=1; displayLevel=1; outFileName=stdoutmark; }

    /* command switches */
    for(argNb=1; argNb<argCount; argNb++) {
        const char* argument = argv[argNb];
        if(!argument) continue;   /* Protection if argument empty */

        if (nextArgumentIsFile==0) {

            /* long commands (--long-word) */
            if (!strcmp(argument, "--")) { nextArgumentIsFile=1; continue; }
            if (!strcmp(argument, "--decompress")) { decode=1; continue; }
            if (!strcmp(argument, "--force")) {  FIO_overwriteMode(); continue; }
            if (!strcmp(argument, "--version")) { displayOut=stdout; DISPLAY(WELCOME_MESSAGE); CLEAN_RETURN(0); }
            if (!strcmp(argument, "--help")) { displayOut=stdout; CLEAN_RETURN(usage_advanced(programName)); }
            if (!strcmp(argument, "--verbose")) { displayLevel++; continue; }
            if (!strcmp(argument, "--quiet")) { displayLevel--; continue; }
            if (!strcmp(argument, "--stdout")) { forceStdout=1; outFileName=stdoutmark; displayLevel-=(displayLevel==2); continue; }
            if (!strcmp(argument, "--ultra")) { ultra=1; continue; }
            if (!strcmp(argument, "--check")) { FIO_setChecksumFlag(2); continue; }
            if (!strcmp(argument, "--no-check")) { FIO_setChecksumFlag(0); continue; }
            if (!strcmp(argument, "--no-dictID")) { FIO_setDictIDFlag(0); continue; }
            if (!strcmp(argument, "--sparse")) { FIO_setSparseWrite(2); continue; }
            if (!strcmp(argument, "--no-sparse")) { FIO_setSparseWrite(0); continue; }
            if (!strcmp(argument, "--test")) { testmode=1; decode=1; continue; }
            if (!strcmp(argument, "--train")) { dictBuild=1; outFileName=g_defaultDictName; continue; }
            if (!strcmp(argument, "--maxdict")) { nextArgumentIsMaxDict=1; lastCommand=1; continue; }
            if (!strcmp(argument, "--dictID")) { nextArgumentIsDictID=1; lastCommand=1; continue; }
            if (!strcmp(argument, "--keep")) { FIO_setRemoveSrcFile(0); continue; }
            if (!strcmp(argument, "--rm")) { FIO_setRemoveSrcFile(1); continue; }

            /* '-' means stdin/stdout */
            if (!strcmp(argument, "-")){
                if (!filenameIdx) {
                    filenameIdx=1, filenameTable[0]=stdinmark;
                    outFileName=stdoutmark;
                    displayLevel-=(displayLevel==2);
                    continue;
            }   }

            /* Decode commands (note : aggregated commands are allowed) */
            if (argument[0]=='-') {
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

                         /* Decoding */
                    case 'd': decode=1; argument++; break;

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
                    case 't': testmode=1; decode=1; argument++; break;

                        /* destination file name */
                    case 'o': nextArgumentIsOutFileName=1; lastCommand=1; argument++; break;

#ifdef UTIL_HAS_CREATEFILELIST
                        /* recursive */
                    case 'r': recursive=1; argument++; break;
#endif

#ifndef ZSTD_NOBENCH
                        /* Benchmark */
                    case 'b': bench=1; argument++; break;

                        /* range bench (benchmark only) */
                    case 'e':
                            /* compression Level */
                            argument++;
                            cLevelLast = readU32FromChar(&argument);
                            break;

                        /* Modify Nb Iterations (benchmark only) */
                    case 'i':
                        argument++;
                        {   U32 const iters = readU32FromChar(&argument);
                            BMK_setNotificationLevel(displayLevel);
                            BMK_SetNbIterations(iters);
                        }
                        break;

                        /* cut input into blocks (benchmark only) */
                    case 'B':
                        argument++;
                        {   size_t bSize = readU32FromChar(&argument);
                            if (toupper(*argument)=='K') bSize<<=10, argument++;  /* allows using KB notation */
                            if (toupper(*argument)=='M') bSize<<=20, argument++;
                            if (toupper(*argument)=='B') argument++;
                            BMK_setNotificationLevel(displayLevel);
                            BMK_SetBlockSize(bSize);
                        }
                        break;
#endif   /* ZSTD_NOBENCH */

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
                if (toupper(*argument)=='K') maxDictSize <<= 10;
                if (toupper(*argument)=='M') maxDictSize <<= 20;
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
    if (bench) {
#ifndef ZSTD_NOBENCH
        BMK_setNotificationLevel(displayLevel);
        BMK_benchFiles(filenameTable, filenameIdx, dictFileName, cLevel, cLevelLast);
#endif
        goto _end;
    }

    /* Check if dictionary builder is selected */
    if (dictBuild) {
#ifndef ZSTD_NODICT
        ZDICT_params_t dictParams;
        memset(&dictParams, 0, sizeof(dictParams));
        dictParams.compressionLevel = dictCLevel;
        dictParams.selectivityLevel = dictSelect;
        dictParams.notificationLevel = displayLevel;
        dictParams.dictID = dictID;
        DiB_trainFromFiles(outFileName, maxDictSize, filenameTable, filenameIdx, dictParams);
#endif
        goto _end;
    }

    /* No input filename ==> use stdin and stdout */
    filenameIdx += !filenameIdx;   /* filenameTable[0] is stdin by default */
    if (!strcmp(filenameTable[0], stdinmark) && !outFileName) outFileName = stdoutmark;   /* when input is stdin, default output is stdout */

    /* Check if input/output defined as console; trigger an error in this case */
    if (!strcmp(filenameTable[0], stdinmark) && IS_CONSOLE(stdin) ) CLEAN_RETURN(badusage(programName));
    if (outFileName && !strcmp(outFileName, stdoutmark) && IS_CONSOLE(stdout) && strcmp(filenameTable[0], stdinmark) && !(forceStdout && decode))
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

    /* No warning message in pipe mode (stdin + stdout) or multi-files mode */
    if (!strcmp(filenameTable[0], stdinmark) && outFileName && !strcmp(outFileName,stdoutmark) && (displayLevel==2)) displayLevel=1;
    if ((filenameIdx>1) & (displayLevel==2)) displayLevel=1;

    /* IO Stream/File */
    FIO_setNotificationLevel(displayLevel);
    if (!decode) {
#ifndef ZSTD_NOCOMPRESS
        if ((filenameIdx==1) && outFileName)
          operationResult = FIO_compressFilename(outFileName, filenameTable[0], dictFileName, cLevel);
        else
          operationResult = FIO_compressMultipleFilenames(filenameTable, filenameIdx, outFileName ? outFileName : ZSTD_EXTENSION, dictFileName, cLevel);
#else
        DISPLAY("Compression not supported\n");
#endif
    } else {  /* decompression */
#ifndef ZSTD_NODECOMPRESS
        if (testmode) { outFileName=nulmark; FIO_setRemoveSrcFile(0); } /* test mode */
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
