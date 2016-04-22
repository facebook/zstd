/*
  zstdcli - Command Line Interface (cli) for zstd
  Copyright (C) Yann Collet 2014-2016

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
/*
  Note : this is a user program, not part of libzstd.
  The license of libzstd is BSD.
  The license of this command line program is GPLv2.
*/


/*-************************************
*  Compiler Options
**************************************/
#define _CRT_SECURE_NO_WARNINGS  /* Visual : removes warning from strcpy */
#define _POSIX_SOURCE 1          /* triggers fileno() within <stdio.h> on unix */


/*-************************************
*  Includes
**************************************/
#include <stdio.h>    /* fprintf, getchar */
#include <stdlib.h>   /* exit, calloc, free */
#include <string.h>   /* strcmp, strlen */
#include <ctype.h>    /* toupper */
#include "fileio.h"
#ifndef ZSTD_NOBENCH
#  include "bench.h"  /* BMK_benchFiles, BMK_SetNbIterations */
#endif
#include "zstd_static.h" /* ZSTD_maxCLevel, ZSTD version numbers  */
#ifndef ZSTD_NODICT
#  include "dibio.h"  /* BMK_benchFiles, BMK_SetNbIterations */
#endif


/*-************************************
*  OS-specific Includes
**************************************/
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>    /* _O_BINARY */
#  include <io.h>       /* _setmode, _isatty */
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), _O_BINARY)
#  define IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#else
#  include <unistd.h>   /* isatty */
#  define SET_BINARY_MODE(file)
#  define IS_CONSOLE(stdStream) isatty(fileno(stdStream))
#endif


/*-************************************
*  Constants
**************************************/
#define COMPRESSOR_NAME "zstd command line interface"
#ifndef ZSTD_VERSION
#  define ZSTD_VERSION "v" ZSTD_VERSION_STRING
#endif
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %i-bits %s, by %s ***\n", COMPRESSOR_NAME, (int)(sizeof(void*)*8), ZSTD_VERSION, AUTHOR

#define ZSTD_EXTENSION ".zst"
#define ZSTD_CAT "zstdcat"
#define ZSTD_UNZSTD "unzstd"

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

static const char*    g_defaultDictName = "dictionary";
static const unsigned g_defaultMaxDictSize = 110 KB;
static const unsigned g_defaultDictCLevel = 5;
static const unsigned g_defaultSelectivityLevel = 9;


/*-************************************
*  Display Macros
**************************************/
#define DISPLAY(...)           fprintf(displayOut, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...)   if (displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static FILE* displayOut;
static unsigned displayLevel = 2;   /* 0 : no display,  1: errors,  2 : + result + interaction + warnings,  3 : + progression,  4 : + information */


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
    DISPLAY( " -#     : # compression level (1-%u, default:1) \n", ZSTD_maxCLevel());
#endif
    DISPLAY( " -d     : decompression \n");
    DISPLAY( " -D file: use `file` as Dictionary \n");
    DISPLAY( " -o file: result stored into `file` (only if 1 input file) \n");
    DISPLAY( " -f     : overwrite output without prompting \n");
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
    DISPLAY( " -t     : test compressed file integrity \n");
    DISPLAY( " -v     : verbose mode\n");
    DISPLAY( " -q     : suppress warnings; specify twice to suppress errors too\n");
    DISPLAY( " -c     : force write to standard output, even if it is the console\n");
#ifndef ZSTD_NOCOMPRESS
    DISPLAY( "--ultra : enable ultra modes (requires more memory to decompress)\n");
#endif
#ifndef ZSTD_NODICT
    DISPLAY( "Dictionary builder :\n");
    DISPLAY( "--train : create a dictionary from a training set of files \n");
    DISPLAY( " -o file: `file` is dictionary name (default: %s) \n", g_defaultDictName);
    DISPLAY( "--maxdict:limit dictionary to specified size (default : %u) \n", g_defaultMaxDictSize);
    DISPLAY( " -s#    : dictionary selectivity level (default: %u)\n", g_defaultSelectivityLevel);
#endif
#ifndef ZSTD_NOBENCH
    DISPLAY( "Benchmark arguments :\n");
    DISPLAY( " -b#    : benchmark file(s), using # compression level (default : 1) \n");
    DISPLAY( " -r#    : test all compression levels from -bX to # (default: 1)\n");
    DISPLAY( " -i#    : iteration loops [1-9](default : 3)\n");
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


#define CLEAN_RETURN(i) { operationResult = (i); goto _end; }

int main(int argCount, const char** argv)
{
    int i,
        bench=0,
        decode=0,
        forceStdout=0,
        main_pause=0,
        nextEntryIsDictionary=0,
        operationResult=0,
        dictBuild=0,
        nextArgumentIsOutFileName=0,
        nextArgumentIsMaxDict=0;
    unsigned cLevel = 1;
    unsigned cLevelLast = 1;
    const char** filenameTable = (const char**)malloc(argCount * sizeof(const char*));   /* argCount >= 1 */
    unsigned filenameIdx = 0;
    const char* programName = argv[0];
    const char* outFileName = NULL;
    const char* dictFileName = NULL;
    char* dynNameSpace = NULL;
    unsigned maxDictSize = g_defaultMaxDictSize;
    unsigned dictCLevel = g_defaultDictCLevel;
    unsigned dictSelect = g_defaultSelectivityLevel;

    /* init */
    (void)cLevelLast; (void)dictCLevel;   /* not used when ZSTD_NOBENCH / ZSTD_NODICT set */
    (void)decode; (void)cLevel; /* not used when ZSTD_NOCOMPRESS set */
    if (filenameTable==NULL) { DISPLAY("not enough memory\n"); exit(1); }
    displayOut = stderr;
    /* Pick out program name from path. Don't rely on stdlib because of conflicting behavior */
    for (i = (int)strlen(programName); i > 0; i--) { if (programName[i] == '/') { i++; break; } }
    programName += i;

    /* preset behaviors */
    if (!strcmp(programName, ZSTD_UNZSTD)) decode=1;
    if (!strcmp(programName, ZSTD_CAT)) { decode=1; forceStdout=1; displayLevel=1; outFileName=stdoutmark; }

    /* command switches */
    for(i=1; i<argCount; i++) {
        const char* argument = argv[i];
        if(!argument) continue;   /* Protection if argument empty */

        /* long commands (--long-word) */
        if (!strcmp(argument, "--decompress")) { decode=1; continue; }
        if (!strcmp(argument, "--force")) {  FIO_overwriteMode(); continue; }
        if (!strcmp(argument, "--version")) { displayOut=stdout; DISPLAY(WELCOME_MESSAGE); CLEAN_RETURN(0); }
        if (!strcmp(argument, "--help")) { displayOut=stdout; CLEAN_RETURN(usage_advanced(programName)); }
        if (!strcmp(argument, "--verbose")) { displayLevel=4; continue; }
        if (!strcmp(argument, "--quiet")) { displayLevel--; continue; }
        if (!strcmp(argument, "--stdout")) { forceStdout=1; outFileName=stdoutmark; displayLevel=1; continue; }
        if (!strcmp(argument, "--test")) { decode=1; outFileName=nulmark; FIO_overwriteMode(); continue; }
        if (!strcmp(argument, "--train")) { dictBuild=1; outFileName=g_defaultDictName; continue; }
        if (!strcmp(argument, "--maxdict")) { nextArgumentIsMaxDict=1; continue; }
        if (!strcmp(argument, "--keep")) { continue; }   /* does nothing, since preserving input is default; for gzip/xz compatibility */
        if (!strcmp(argument, "--ultra")) { FIO_setMaxWLog(0); continue; }

        /* '-' means stdin/stdout */
        if (!strcmp(argument, "-")){
            if (!filenameIdx) { filenameIdx=1, filenameTable[0]=stdinmark; outFileName=stdoutmark; continue; }
        }

        /* Decode commands (note : aggregated commands are allowed) */
        if (argument[0]=='-') {
            argument++;

            while (argument[0]!=0) {
#ifndef ZSTD_NOCOMPRESS
                /* compression Level */
                if ((*argument>='0') && (*argument<='9')) {
                    cLevel = 0;
                    while ((*argument >= '0') && (*argument <= '9')) {
                        cLevel *= 10;
                        cLevel += *argument - '0';
                        argument++;
                    }
                    dictCLevel = cLevel;
                    if (dictCLevel > ZSTD_maxCLevel())
                        CLEAN_RETURN(badusage(programName));
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
                case 'c': forceStdout=1; outFileName=stdoutmark; displayLevel=1; argument++; break;

                    /* Use file content as dictionary */
                case 'D': nextEntryIsDictionary = 1; argument++; break;

                    /* Overwrite */
                case 'f': FIO_overwriteMode(); argument++; break;

                    /* Verbose mode */
                case 'v': displayLevel=4; argument++; break;

                    /* Quiet mode */
                case 'q': displayLevel--; argument++; break;

                    /* keep source file (default anyway, so useless; for gzip/xz compatibility) */
                case 'k': argument++; break;

                    /* test compressed file */
                case 't': decode=1; outFileName=nulmark; FIO_overwriteMode(); argument++; break;

                    /* dictionary name */
                case 'o': nextArgumentIsOutFileName=1; argument++; break;

#ifndef ZSTD_NOBENCH
                    /* Benchmark */
                case 'b': bench=1; argument++; break;

                    /* Modify Nb Iterations (benchmark only) */
                case 'i':
                    {   U32 iters= 0;
                        argument++;
                        while ((*argument >='0') && (*argument <='9'))
                            iters *= 10, iters += *argument++ - '0';
                        BMK_setNotificationLevel(displayLevel);
                        BMK_SetNbIterations(iters);
                    }
                    break;

                    /* cut input into blocks (benchmark only) */
                case 'B':
                    {   size_t bSize = 0;
                        argument++;
                        while ((*argument >='0') && (*argument <='9'))
                            bSize *= 10, bSize += *argument++ - '0';
                        if (toupper(*argument)=='K') bSize<<=10, argument++;  /* allows using KB notation */
                        if (toupper(*argument)=='M') bSize<<=20, argument++;
                        if (toupper(*argument)=='B') argument++;
                        BMK_setNotificationLevel(displayLevel);
                        BMK_SetBlockSize(bSize);
                    }
                    break;

                    /* range bench (benchmark only) */
                case 'r':
                        /* compression Level */
                        argument++;
                        if ((*argument>='0') && (*argument<='9')) {
                            cLevelLast = 0;
                            while ((*argument >= '0') && (*argument <= '9'))
                                cLevelLast *= 10, cLevelLast += *argument++ - '0';
                        }
                        break;
#endif   /* ZSTD_NOBENCH */

                    /* Selection level */
                case 's': argument++;
                    dictSelect = 0;
                    while ((*argument >= '0') && (*argument <= '9'))
                        dictSelect *= 10, dictSelect += *argument++ - '0';
                    break;

                    /* Pause at the end (-p) or set an additional param (-p#) (hidden option) */
                case 'p': argument++;
#ifndef ZSTD_NOBENCH
                    if ((*argument>='0') && (*argument<='9')) {
                        int additionalParam = 0;
                        while ((*argument >= '0') && (*argument <= '9'))
                            additionalParam *= 10, additionalParam += *argument++ - '0';
                        BMK_setAdditionalParam(additionalParam);
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

        if (nextEntryIsDictionary) {
            nextEntryIsDictionary = 0;
            dictFileName = argument;
            continue;
        }

        if (nextArgumentIsOutFileName) {
            nextArgumentIsOutFileName = 0;
            outFileName = argument;
            if (!strcmp(outFileName, "-")) outFileName = stdoutmark;
            continue;
        }

        if (nextArgumentIsMaxDict) {
            nextArgumentIsMaxDict = 0;
            maxDictSize = 0;
            while ((*argument>='0') && (*argument<='9'))
                maxDictSize = maxDictSize * 10 + (*argument - '0'), argument++;
            if (toupper(*argument)=='K') maxDictSize <<= 10;
            continue;
        }

        /* add filename to list */
        filenameTable[filenameIdx++] = argument;
    }

    /* Welcome message (if verbose) */
    DISPLAYLEVEL(3, WELCOME_MESSAGE);

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
        dictParams.compressionLevel = dictCLevel;
        dictParams.selectivityLevel = dictSelect;
        dictParams.notificationLevel = displayLevel;
        DiB_trainFromFiles(outFileName, maxDictSize, filenameTable, filenameIdx, dictParams);
#endif
        goto _end;
    }

    /* No input filename ==> use stdin and stdout */
    if(!filenameIdx) filenameIdx=1, filenameTable[0]=stdinmark, outFileName=stdoutmark;

    /* Check if input/output defined as console; trigger an error in this case */
    if (!strcmp(filenameTable[0], stdinmark) && IS_CONSOLE(stdin) ) CLEAN_RETURN(badusage(programName));
    if (outFileName && !strcmp(outFileName, stdoutmark) && IS_CONSOLE(stdout) && !forceStdout) CLEAN_RETURN(badusage(programName));

    /* user-selected output filename, only possible with a single file */
    if (outFileName && strcmp(outFileName,stdoutmark) && strcmp(outFileName,nulmark) && (filenameIdx>1)) {
        DISPLAY("Too many files (%u) on the command line. \n", filenameIdx);
        CLEAN_RETURN(filenameIdx);
    }

    /* No warning message in pipe mode (stdin + stdout) or multiple mode */
    if (!strcmp(filenameTable[0], stdinmark) && outFileName && !strcmp(outFileName,stdoutmark) && (displayLevel==2)) displayLevel=1;
    if ((filenameIdx>1) && (displayLevel==2)) displayLevel=1;

    /* IO Stream/File */
    FIO_setNotificationLevel(displayLevel);
#ifndef ZSTD_NOCOMPRESS
    if (!decode) {
        if (filenameIdx==1 && outFileName)
          operationResult = FIO_compressFilename(outFileName, filenameTable[0], dictFileName, cLevel);
        else
          operationResult = FIO_compressMultipleFilenames(filenameTable, filenameIdx, outFileName ? outFileName : ZSTD_EXTENSION, dictFileName, cLevel);
    } else 
#endif
    {  /* decompression */
        if (filenameIdx==1 && outFileName)
        operationResult = FIO_decompressFilename(outFileName, filenameTable[0], dictFileName);
        else
        operationResult = FIO_decompressMultipleFilenames(filenameTable, filenameIdx, outFileName ? outFileName : ZSTD_EXTENSION, dictFileName);
    }

_end:
    if (main_pause) waitEnter();
    free(dynNameSpace);
    free((void*)filenameTable);
    return operationResult;
}
