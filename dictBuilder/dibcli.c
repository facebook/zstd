/*
  dibcli - Command Line Interface (cli) for Dictionary Builder
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
  - zstd source repository : https://github.com/Cyan4973/zstd
  - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
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

#include "dictBuilder.h"


/**************************************
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


/**************************************
*  Constants
**************************************/
#define PROGRAM_DESCRIPTION "Dictionary builder"
#ifndef PROGRAM_VERSION
#  define QUOTE(str) #str
#  define EXP_Q(str) QUOTE(str)
#  define PROGRAM_VERSION "v" EXP_Q(DiB_VERSION_MAJOR) "." EXP_Q(DiB_VERSION_MINOR) "." EXP_Q(DiB_VERSION_RELEASE)
#endif
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %s %i-bits, by %s ***\n", PROGRAM_DESCRIPTION, PROGRAM_VERSION, (int)(sizeof(void*)*8), AUTHOR

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

static const unsigned selectionLevelDefault = 9;     /* determined experimentally */
static const unsigned maxDictSizeDefault = 110 KB;
static const char* dictFileNameDefault = "dictionary";


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
    DISPLAY( "      %s [arg] [filenames]\n", programName);
    DISPLAY( "\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( "--maxdict : limit dictionary to specified size (default : %u) \n", maxDictSizeDefault);
    DISPLAY( " -o       : name of dictionary file (default: %s) \n", dictFileNameDefault);
    DISPLAY( " -h/-H    : display help/long help and exit\n");
    return 0;
}

static int usage_advanced(const char* programName)
{
    DISPLAY(WELCOME_MESSAGE);
    usage(programName);
    DISPLAY( "\n");
    DISPLAY( "Advanced arguments :\n");
    DISPLAY( " -#     : selection level # (default :%u)\n", selectionLevelDefault);
    DISPLAY( " -V     : display Version number and exit\n");
    DISPLAY( " -v     : verbose mode\n");
    DISPLAY( " -q     : suppress warnings; specify twice to suppress errors too\n");
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


int main(int argCount, const char** argv)
{
    int i,
        main_pause=0,
        operationResult=0,
        nextArgumentIsMaxDict=0,
        nextArgumentIsDictFileName=0;
    size_t maxDictSize = maxDictSizeDefault;
    unsigned selectionLevel = selectionLevelDefault;
    const char** filenameTable = (const char**)malloc(argCount * sizeof(const char*));   /* argCount >= 1 */
    unsigned filenameIdx = 0;
    const char* programName = argv[0];
    const char* dictFileName = dictFileNameDefault;

    /* init */
    displayOut = stderr;   /* unfortunately, cannot be set at declaration */
    if (filenameTable==NULL) EXM_THROW(1, "not enough memory\n");
    /* Pick out program name from path. Don't rely on stdlib because of conflicting behavior */
    for (i = (int)strlen(programName); i > 0; i--) { if ((programName[i] == '/') || (programName[i] == '\\')) { i++; break; } }
    programName += i;

    /* command switches */
    for(i=1; i<argCount; i++) {
        const char* argument = argv[i];

        if(!argument) continue;   /* Protection if argument empty */

        if (nextArgumentIsDictFileName) {
            nextArgumentIsDictFileName=0;
            dictFileName = argument;
            continue;
        }

        if (nextArgumentIsMaxDict) {
            nextArgumentIsMaxDict = 0;
            maxDictSize = 0;
            while ((*argument>='0') && (*argument<='9'))
                maxDictSize = maxDictSize * 10 + (*argument - '0'), argument++;
            if (*argument=='k' || *argument=='K')
                maxDictSize <<= 10;
            continue;
        }

        /* long commands (--long-word) */
        if (!strcmp(argument, "--version")) { displayOut=stdout; DISPLAY(WELCOME_MESSAGE); return 0; }
        if (!strcmp(argument, "--help")) { displayOut=stdout; return usage_advanced(programName); }
        if (!strcmp(argument, "--verbose")) { displayLevel=4; continue; }
        if (!strcmp(argument, "--quiet")) { displayLevel--; continue; }
        if (!strcmp(argument, "--maxdict")) { nextArgumentIsMaxDict=1; continue; }

        /* Decode commands (note : aggregated commands are allowed) */
        if (argument[0]=='-') {
            argument++;

            while (argument[0]!=0) {
                /* selection Level */
                if ((*argument>='0') && (*argument<='9')) {
                    selectionLevel = 0;
                    while ((*argument >= '0') && (*argument <= '9')) {
                        selectionLevel *= 10;
                        selectionLevel += *argument - '0';
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

                    /* Verbose mode */
                case 'v': displayLevel++; if (displayLevel<3) displayLevel=3; argument++; break;

                    /* Quiet mode */
                case 'q': displayLevel--; argument++; break;

                    /* dictionary name */
                case 'o': nextArgumentIsDictFileName=1; argument++; break;

                    /* Pause at the end (hidden option) */
                case 'p': main_pause=1; argument++; break;

                    /* unknown command */
                default : return badusage(programName);
            }   }
            continue;
        }

        /* add filename to list */
        filenameTable[filenameIdx++] = argument;
    }

    /* Welcome message (if verbose) */
    DISPLAYLEVEL(3, WELCOME_MESSAGE);

    if (filenameIdx==0) return badusage(programName);

    /* building ... */
    DiB_setNotificationLevel(displayLevel);
    operationResult = DiB_trainDictionary(dictFileName, maxDictSize, selectionLevel, filenameTable, filenameIdx);

    if (main_pause) waitEnter();
    free((void*)filenameTable);
    return operationResult;
}
