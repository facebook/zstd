/*
    datagen.c - compressible data generator test tool
    Copyright (C) Yann Collet 2012-2015

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
   - ZSTD source repository : https://github.com/Cyan4973/zstd
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/**************************************
*  Remove Visual warning messages
**************************************/
#define _CRT_SECURE_NO_WARNINGS   /* fgets */


/**************************************
*  Includes
**************************************/
#include <stdlib.h>    /* malloc */
#include <stdio.h>     /* fgets, sscanf */
#include <string.h>    /* strcmp */


/**************************************
*  Basic Types
**************************************/
#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)   /* C99 */
# include <stdint.h>
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
#else
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
#endif


/**************************************
*  OS-specific Includes
**************************************/
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>   /* _O_BINARY */
#  include <io.h>      /* _setmode, _isatty */
#  ifdef __MINGW32__
   int _fileno(FILE *stream);   /* MINGW somehow forgets to include this windows declaration into <stdio.h> */
#  endif
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), _O_BINARY)
#  define IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#else
#  include <unistd.h>  /* isatty */
#  define SET_BINARY_MODE(file)
#  define IS_CONSOLE(stdStream) isatty(fileno(stdStream))
#endif


/**************************************
*  Constants
**************************************/
#ifndef ZSTD_VERSION
#  define ZSTD_VERSION "r0"
#endif

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define CDG_SIZE_DEFAULT (64 KB)
#define CDG_SEED_DEFAULT 0
#define CDG_COMPRESSIBILITY_DEFAULT 50
#define PRIME1   2654435761U
#define PRIME2   2246822519U


/**************************************
*  Macros
**************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (displayLevel>=l) { DISPLAY(__VA_ARGS__); }


/**************************************
*  Local Parameters
**************************************/
static unsigned no_prompt = 0;
static unsigned displayLevel = 2;


/*********************************************************
*  Local Functions
*********************************************************/
#define CDG_rotl32(x,r) ((x << r) | (x >> (32 - r)))
static unsigned int CDG_rand(U32* src)
{
    U32 rand32 = *src;
    rand32 *= PRIME1;
    rand32 ^= PRIME2;
    rand32  = CDG_rotl32(rand32, 13);
    *src = rand32;
    return rand32;
}


#define LTSIZE 8192
#define LTMASK (LTSIZE-1)
static void* CDG_createLiteralDistrib(double ld)
{
    BYTE* lt = malloc(LTSIZE);
    U32 i = 0;
    BYTE character = '0';
    BYTE firstChar = '(';
    BYTE lastChar = '}';

    if (ld==0.0)
    {
        character = 0;
        firstChar = 0;
        lastChar =255;
    }
    while (i<LTSIZE)
    {
        U32 weight = (U32)((double)(LTSIZE - i) * ld) + 1;
        U32 end;
        if (weight + i > LTSIZE) weight = LTSIZE-i;
        end = i + weight;
        while (i < end) lt[i++] = character;
        character++;
        if (character > lastChar) character = firstChar;
    }
    return lt;
}

static char CDG_genChar(U32* seed, const void* ltctx)
{
    const BYTE* lt = ltctx;
    U32 id = CDG_rand(seed) & LTMASK;
    return lt[id];
}

#define CDG_RAND15BITS  ((CDG_rand(seed) >> 3) & 32767)
#define CDG_RANDLENGTH  ( ((CDG_rand(seed) >> 7) & 7) ? (CDG_rand(seed) & 15) : (CDG_rand(seed) & 511) + 15)
#define CDG_DICTSIZE    (32 KB)
static void CDG_generate(U64 size, U32* seed, double matchProba, double litProba)
{
    BYTE fullbuff[CDG_DICTSIZE + 128 KB + 1];
    BYTE* buff = fullbuff + CDG_DICTSIZE;
    U64 total=0;
    U32 P32 = (U32)(32768 * matchProba);
    U32 pos=1;
    U32 genBlockSize = 128 KB;
    void* ldctx = CDG_createLiteralDistrib(litProba);
    FILE* fout = stdout;

    /* init */
    SET_BINARY_MODE(stdout);
    fullbuff[0] = CDG_genChar(seed, ldctx);
    while (pos<32 KB)
    {
        /* Select : Literal (char) or Match (within 32K) */
        if (CDG_RAND15BITS < P32)
        {
            /* Copy (within 64K) */
            U32 d;
            int ref;
            int length = CDG_RANDLENGTH + 4;
            U32 offset = CDG_RAND15BITS + 1;
            if (offset > pos) offset = pos;
            ref = pos - offset;
            d = pos + length;
            while (pos < d) fullbuff[pos++] = fullbuff[ref++];
        }
        else
        {
            /* Literal (noise) */
            U32 d = pos + CDG_RANDLENGTH;
            while (pos < d) fullbuff[pos++] = CDG_genChar(seed, ldctx);
        }
    }

    /* Generate compressible data */
    pos = 0;
    while (total < size)
    {
        if (size-total < 128 KB) genBlockSize = (U32)(size-total);
        total += genBlockSize;
        buff[genBlockSize] = 0;
        pos = 0;
        while (pos<genBlockSize)
        {
            /* Select : Literal (char) or Match (within 32K) */
            if (CDG_RAND15BITS < P32)
            {
                /* Copy (within 64K) */
                int ref;
                U32 d;
                int length = CDG_RANDLENGTH + 4;
                U32 offset = CDG_RAND15BITS + 1;
                if (pos + length > genBlockSize ) length = genBlockSize - pos;
                ref = pos - offset;
                d = pos + length;
                while (pos < d) buff[pos++] = buff[ref++];
            }
            else
            {
                /* Literal (noise) */
                U32 d;
                int length = CDG_RANDLENGTH;
                if (pos + length > genBlockSize) length = genBlockSize - pos;
                d = pos + length;
                while (pos < d) buff[pos++] = CDG_genChar(seed, ldctx);
            }
        }

        /* output generated data */
        fwrite(buff, 1, genBlockSize, fout);
        /* Regenerate prefix */
        memcpy(fullbuff, buff + 96 KB, 32 KB);
    }
}


/*********************************************************
*  Command line
*********************************************************/
static int CDG_usage(char* programName)
{
    DISPLAY( "Compressible data generator\n");
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [size] [args]\n", programName);
    DISPLAY( "\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( " -g#    : generate # data (default:%i)\n", CDG_SIZE_DEFAULT);
    DISPLAY( " -s#    : Select seed (default:%i)\n", CDG_SEED_DEFAULT);
    DISPLAY( " -p#    : Select compressibility in %% (default:%i%%)\n", CDG_COMPRESSIBILITY_DEFAULT);
    DISPLAY( " -h     : display help and exit\n");
    return 0;
}


int main(int argc, char** argv)
{
    int argNb;
    double proba = (double)CDG_COMPRESSIBILITY_DEFAULT / 100;
    double litProba = proba / 3.6;
    U64 size = CDG_SIZE_DEFAULT;
    U32 seed = CDG_SEED_DEFAULT;
    char* programName;

    /* Check command line */
    programName = argv[0];
    for(argNb=1; argNb<argc; argNb++)
    {
        char* argument = argv[argNb];

        if(!argument) continue;   /* Protection if argument empty */

        /* Handle commands. Aggregated commands are allowed */
        if (*argument=='-')
        {
            if (!strcmp(argument, "--no-prompt")) { no_prompt=1; continue; }

            argument++;
            while (*argument!=0)
            {
                switch(*argument)
                {
                case 'h':
                    return CDG_usage(programName);
                case 'g':
                    argument++;
                    size=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        size *= 10;
                        size += *argument - '0';
                        argument++;
                    }
                    if (*argument=='K') { size <<= 10; argument++; }
                    if (*argument=='M') { size <<= 20; argument++; }
                    if (*argument=='G') { size <<= 30; argument++; }
                    if (*argument=='B') { argument++; }
                    break;
                case 's':
                    argument++;
                    seed=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        seed *= 10;
                        seed += *argument - '0';
                        argument++;
                    }
                    break;
                case 'P':
                    argument++;
                    proba=0.0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        proba *= 10;
                        proba += *argument - '0';
                        argument++;
                    }
                    if (proba>100.) proba=100.;
                    proba /= 100.;
                    litProba = proba / 4.;
                    break;
                case 'L':
                    argument++;
                    litProba=0.;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        litProba *= 10;
                        litProba += *argument - '0';
                        argument++;
                    }
                    if (litProba>100.) litProba=100.;
                    litProba /= 100.;
                    break;
                case 'v':
                    displayLevel = 4;
                    argument++;
                    break;
                default:
                    return CDG_usage(programName);
                }
            }

        }
    }

    DISPLAYLEVEL(4, "Data Generator %s \n", ZSTD_VERSION);
    DISPLAYLEVEL(3, "Seed = %u \n", seed);
    if (proba!=CDG_COMPRESSIBILITY_DEFAULT) DISPLAYLEVEL(3, "Compressibility : %i%%\n", (U32)(proba*100));

    CDG_generate(size, &seed, proba, litProba);

    return 0;
}
