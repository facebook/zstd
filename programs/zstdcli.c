/*
  zstdcli - Command Line Interface (cli) for zstd
  Copyright (C) Yann Collet 2014-2015

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
  - zstd source repository : https://github.com/Cyan4973/zstd
  - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/
/*
  Note : this is user program.
  It is not part of zstd compression library.
  The license of this compression CLI program is GPLv2.
  The license of zstd library is BSD.
*/


/**************************************
*  Compiler Options
**************************************/
#define _CRT_SECURE_NO_WARNINGS  /* Visual : removes warning from strcpy */
#define _POSIX_SOURCE 1          /* triggers fileno() within <stdio.h> on unix */


/**************************************
*  Includes
**************************************/
#include <stdio.h>    /* fprintf, getchar */
#include <stdlib.h>   /* exit, calloc, free */
#include <string.h>   /* strcmp, strlen */
#include "bench.h"    /* BMK_benchFiles, BMK_SetNbIterations */
#include "fileio.h"


/**************************************
*  OS-specific Includes
**************************************/
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>    // _O_BINARY
#  include <io.h>       // _setmode, _isatty
#  ifdef __MINGW32__
   /* int _fileno(FILE *stream);   // seems no longer useful // MINGW somehow forgets to include this windows declaration into <stdio.h> */
#  endif
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), _O_BINARY)
#  define IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#else
#  include <unistd.h>   // isatty
#  define SET_BINARY_MODE(file)
#  define IS_CONSOLE(stdStream) isatty(fileno(stdStream))
#endif


/**************************************
*  Constants
**************************************/
#define COMPRESSOR_NAME "zstd command line interface"
#ifndef ZSTD_VERSION
#  define ZSTD_VERSION "v0.3.3"
#endif
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %i-bits %s, by %s (%s) ***\n", COMPRESSOR_NAME, (int)(sizeof(void*)*8), ZSTD_VERSION, AUTHOR, __DATE__
#define ZSTD_EXTENSION ".zst"
#define ZSTD_CAT "zstdcat"
#define ZSTD_UNZSTD "unzstd"

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)


/**************************************
*  Display Macros
**************************************/
#define DISPLAY(...)           fprintf(displayOut, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...)   if (displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static FILE* displayOut;
static unsigned displayLevel = 2;   // 0 : no display  // 1: errors  // 2 : + result + interaction + warnings ;  // 3 : + progression;  // 4 : + information


/**************************************
*  Exceptions
**************************************/
#define DEBUG 0
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define EXM_THROW(error, ...)                                             \
{                                                                         \
    DEBUGOUTPUT("Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                                \
    DISPLAYLEVEL(1, __VA_ARGS__);                                         \
    DISPLAYLEVEL(1, "\n");                                                \
    exit(error);                                                          \
}


/**************************************
*  Command Line
**************************************/
static int usage(const char* programName)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [arg] [input] [output]\n", programName);
    DISPLAY( "\n");
    DISPLAY( "input   : a filename\n");
    DISPLAY( "          with no FILE, or when FILE is - , read standard input\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( " -1     : Fast compression (default) \n");
    DISPLAY( " -9     : High compression \n");
    DISPLAY( " -d     : decompression (default for %s extension)\n", ZSTD_EXTENSION);
    //DISPLAY( " -z     : force compression\n");
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
    DISPLAY( " -v     : verbose mode\n");
    DISPLAY( " -q     : suppress warnings; specify twice to suppress errors too\n");
    DISPLAY( " -c     : force write to standard output, even if it is the console\n");
    //DISPLAY( " -t     : test compressed file integrity\n");
    DISPLAY( "Benchmark arguments :\n");
    DISPLAY( " -b#    : benchmark file(s), using # compression level (default : 1) \n");
    DISPLAY( " -B#    : cut file into independent blocks of size # (default : no block)\n");
    DISPLAY( " -i#    : iteration loops [1-9](default : 3)\n");
    DISPLAY( " -r#    : test all compression levels from 1 to # (default : disabled)\n");
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


int main(int argc, char** argv)
{
    int i,
        bench=0,
        decode=0,
        forceStdout=0,
        main_pause=0,
        rangeBench = 1;
    unsigned fileNameStart = 0;
    unsigned nbFiles = 0;
    unsigned cLevel = 1;
    const char* programName = argv[0];
    const char* inFileName = NULL;
    const char* outFileName = NULL;
    char* dynNameSpace = NULL;
    const char extension[] = ZSTD_EXTENSION;

    displayOut = stderr;
    /* Pick out basename component. Don't rely on stdlib because of conflicting behavior. */
    for (i = (int)strlen(programName); i > 0; i--)
    {
        if (programName[i] == '/') { i++; break; }
    }
    programName += i;

    /* zstdcat preset behavior */
    if (!strcmp(programName, ZSTD_CAT)) { decode=1; forceStdout=1; displayLevel=1; outFileName=stdoutmark; }

    /* unzstd preset behavior */
    if (!strcmp(programName, ZSTD_UNZSTD))
        decode=1;

    /* command switches */
    for(i=1; i<argc; i++)
    {
        char* argument = argv[i];

        if(!argument) continue;   /* Protection if argument empty */

        /* long commands (--long-word) */
        if (!strcmp(argument, "--version")) { displayOut=stdout; DISPLAY(WELCOME_MESSAGE); return 0; }
        if (!strcmp(argument, "--help")) { displayOut=stdout; return usage_advanced(programName); }
        if (!strcmp(argument, "--verbose")) { displayLevel=4; continue; }

        /* Decode commands (note : aggregated commands are allowed) */
        if (argument[0]=='-')
        {
            /* '-' means stdin/stdout */
            if (argument[1]==0)
            {
                if (!inFileName) inFileName=stdinmark;
                else outFileName=stdoutmark;
                continue;
            }

            argument++;

            while (argument[0]!=0)
            {
                /* compression Level */
                if ((*argument>='0') && (*argument<='9'))
                {
                    cLevel = 0;
                    while ((*argument >= '0') && (*argument <= '9'))
                    {
                        cLevel *= 10;
                        cLevel += *argument - '0';
                        argument++;
                    }
                    continue;
                }

                switch(argument[0])
                {
                    /* Display help */
                case 'V': displayOut=stdout; DISPLAY(WELCOME_MESSAGE); return 0;   /* Version Only */
                case 'H':
                case 'h': displayOut=stdout; return usage_advanced(programName);

                    /* Compression (default) */
                //case 'z': forceCompress = 1; break;

                    /* Decoding */
                case 'd': decode=1; argument++; break;

                    /* Force stdout, even if stdout==console */
                case 'c': forceStdout=1; outFileName=stdoutmark; displayLevel=1; argument++; break;

                    // Test
                //case 't': decode=1; LZ4IO_setOverwrite(1); output_filename=nulmark; break;

                    /* Overwrite */
                case 'f': FIO_overwriteMode(); argument++; break;

                    /* Verbose mode */
                case 'v': displayLevel=4; argument++; break;

                    /* Quiet mode */
                case 'q': displayLevel--; argument++; break;

                    /* keep source file (default anyway, so useless; only for xz/lzma compatibility) */
                case 'k': argument++; break;

                    /* Benchmark */
                case 'b': bench=1; argument++; break;

                    /* Modify Nb Iterations (benchmark only) */
                case 'i':
                    {
                        int iters= 0;
                        argument++;
                        while ((*argument >='0') && (*argument <='9'))
                            iters *= 10, iters += *argument++ - '0';
                        BMK_SetNbIterations(iters);
                    }
                    break;

                    /* cut input into blocks (benchmark only) */
                case 'B':
                    {
                        size_t bSize = 0;
                        argument++;
                        while ((*argument >='0') && (*argument <='9'))
                            bSize *= 10, bSize += *argument++ - '0';
                        if (*argument=='K') bSize<<=10, argument++;  /* allows using KB notation */
                        if (*argument=='M') bSize<<=20, argument++;
                        if (*argument=='B') argument++;
                        BMK_SetBlockSize(bSize);
                    }
                    break;

                    /* range bench (benchmark only) */
                case 'r':
                        rangeBench = -1;
                        argument++;
                        break;

                    /* Pause at the end (hidden option) */
                case 'p': main_pause=1; argument++; break;

                    /* unknown command */
                default : return badusage(programName);
                }
            }
            continue;
        }

        /* first provided filename is input */
        if (!inFileName) { inFileName = argument; fileNameStart = i; nbFiles = argc-i; continue; }

        /* second provided filename is output */
        if (!outFileName)
        {
            outFileName = argument;
            if (!strcmp (outFileName, nullString)) outFileName = nulmark;
            continue;
        }
    }

    /* Welcome message (if verbose) */
    DISPLAYLEVEL(3, WELCOME_MESSAGE);

    /* No input filename ==> use stdin */
    if(!inFileName) { inFileName=stdinmark; }

    /* Check if input defined as console; trigger an error in this case */
    if (!strcmp(inFileName, stdinmark) && IS_CONSOLE(stdin) ) return badusage(programName);

    /* Check if benchmark is selected */
    if (bench) { BMK_benchFiles(argv+fileNameStart, nbFiles, cLevel*rangeBench); goto _end; }

    /* No output filename ==> try to select one automatically (when possible) */
    while (!outFileName)
    {
        if (!IS_CONSOLE(stdout)) { outFileName=stdoutmark; break; }   /* Default to stdout whenever possible (i.e. not a console) */
        if (!decode)   /* compression to file */
        {
            size_t l = strlen(inFileName);
            dynNameSpace = (char*)calloc(1,l+5);
            if (dynNameSpace==NULL) { DISPLAY("not enough memory\n"); exit(1); }
            strcpy(dynNameSpace, inFileName);
            strcpy(dynNameSpace+l, ZSTD_EXTENSION);
            outFileName = dynNameSpace;
            DISPLAYLEVEL(2, "Compressed filename will be : %s \n", outFileName);
            break;
        }
        /* decompression to file (automatic name will work only if input filename has correct format extension) */
        {
            size_t filenameSize = strlen(inFileName);
            if (strcmp(inFileName + (filenameSize-4), extension))
            {
                 DISPLAYLEVEL(1, "unknown suffix - cannot determine destination filename\n");
                 return badusage(programName);
            }
            dynNameSpace = (char*)calloc(1,filenameSize+1);
            if (dynNameSpace==NULL) { DISPLAY("not enough memory\n"); exit(1); }
            outFileName = dynNameSpace;
            strcpy(dynNameSpace, inFileName);
            dynNameSpace[filenameSize-4]=0;
            DISPLAYLEVEL(2, "Decoding file %s \n", outFileName);
        }
    }

    /* Check if output is defined as console; trigger an error in this case */
    if (!strcmp(outFileName,stdoutmark) && IS_CONSOLE(stdout) && !forceStdout) return badusage(programName);

    /* No warning message in pure pipe mode (stdin + stdout) */
    if (!strcmp(inFileName, stdinmark) && !strcmp(outFileName,stdoutmark) && (displayLevel==2)) displayLevel=1;

    /* IO Stream/File */
    FIO_setNotificationLevel(displayLevel);
    if (decode)
        FIO_decompressFilename(outFileName, inFileName);
    else
        FIO_compressFilename(outFileName, inFileName, cLevel);

_end:
    if (main_pause) waitEnter();
    free(dynNameSpace);
    return 0;
}
