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
*  Includes
**************************************/
#include <stdlib.h>    /* malloc */
#include <stdio.h>     /* FILE, fwrite */
#include <string.h>    /* memcpy */


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
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), _O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif


/**************************************
*  Constants
**************************************/
#define KB *(1 <<10)

#define PRIME1   2654435761U
#define PRIME2   2246822519U


/*********************************************************
*  Local Functions
*********************************************************/
#define RDG_rotl32(x,r) ((x << r) | (x >> (32 - r)))
static unsigned int RDG_rand(U32* src)
{
    U32 rand32 = *src;
    rand32 *= PRIME1;
    rand32 ^= PRIME2;
    rand32  = RDG_rotl32(rand32, 13);
    *src = rand32;
    return rand32;
}


#define LTSIZE 8192
#define LTMASK (LTSIZE-1)
static void* RDG_createLiteralDistrib(double ld)
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

static char RDG_genChar(U32* seed, const void* ltctx)
{
    const BYTE* lt = ltctx;
    U32 id = RDG_rand(seed) & LTMASK;
    return lt[id];
}

#define RDG_RAND15BITS  ((RDG_rand(seed) >> 3) & 32767)
#define RDG_RANDLENGTH  ( ((RDG_rand(seed) >> 7) & 7) ? (RDG_rand(seed) & 15) : (RDG_rand(seed) & 511) + 15)
#define RDG_DICTSIZE    (32 KB)
void RDG_generate(U64 size, U32 seedInit, double matchProba, double litProba)
{
    BYTE fullbuff[RDG_DICTSIZE + 128 KB + 1];
    BYTE* buff = fullbuff + RDG_DICTSIZE;
    U64 total=0;
    U32 P32 = (U32)(32768 * matchProba);
    U32 pos=1;
    U32 genBlockSize = 128 KB;
    void* ldctx = RDG_createLiteralDistrib(litProba);
    FILE* fout = stdout;
    U32* seed = &seedInit;;

    /* init */
    SET_BINARY_MODE(stdout);
    fullbuff[0] = RDG_genChar(seed, ldctx);
    while (pos<32 KB)
    {
        /* Select : Literal (char) or Match (within 32K) */
        if (RDG_RAND15BITS < P32)
        {
            /* Copy (within 64K) */
            U32 d;
            int ref;
            int length = RDG_RANDLENGTH + 4;
            U32 offset = RDG_RAND15BITS + 1;
            if (offset > pos) offset = pos;
            ref = pos - offset;
            d = pos + length;
            while (pos < d) fullbuff[pos++] = fullbuff[ref++];
        }
        else
        {
            /* Literal (noise) */
            U32 d = pos + RDG_RANDLENGTH;
            while (pos < d) fullbuff[pos++] = RDG_genChar(seed, ldctx);
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
            if (RDG_RAND15BITS < P32)
            {
                /* Copy (within 64K) */
                int ref;
                U32 d;
                int length = RDG_RANDLENGTH + 4;
                U32 offset = RDG_RAND15BITS + 1;
                if (pos + length > genBlockSize ) length = genBlockSize - pos;
                ref = pos - offset;
                d = pos + length;
                while (pos < d) buff[pos++] = buff[ref++];
            }
            else
            {
                /* Literal (noise) */
                U32 d;
                int length = RDG_RANDLENGTH;
                if (pos + length > genBlockSize) length = genBlockSize - pos;
                d = pos + length;
                while (pos < d) buff[pos++] = RDG_genChar(seed, ldctx);
            }
        }

        /* output generated data */
        fwrite(buff, 1, genBlockSize, fout);
        /* Regenerate prefix */
        memcpy(fullbuff, buff + 96 KB, 32 KB);
    }
}
