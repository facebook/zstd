/*
    datagen.c - compressible data generator test tool
    Copyright (C) Yann Collet 2012-2016

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
    - source repository : https://github.com/Cyan4973/zstd
*/

/*-************************************
*  Includes
**************************************/
#include <stdlib.h>    /* malloc */
#include <stdio.h>     /* FILE, fwrite */
#include <string.h>    /* memcpy */
#include "mem.h"


/*-************************************
*  OS-specific Includes
**************************************/
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>   /* _O_BINARY */
#  include <io.h>      /* _setmode, _isatty */
#  define SET_BINARY_MODE(file) {int unused = _setmode(_fileno(file), _O_BINARY); (void)unused; }
#else
#  define SET_BINARY_MODE(file)
#endif


/*-************************************
*  Macros
**************************************/
#define KB *(1 <<10)
#define MIN(a,b)  ( (a) < (b) ? (a) : (b) )

#define RDG_DEBUG 0
#define TRACE(...)   if (RDG_DEBUG) fprintf(stderr, __VA_ARGS__ )


/*-************************************
*  Local constants
**************************************/
#define LTLOG 13
#define LTSIZE (1<<LTLOG)
#define LTMASK (LTSIZE-1)


/*-*******************************************************
*  Local Functions
*********************************************************/
#define RDG_rotl32(x,r) ((x << r) | (x >> (32 - r)))
static U32 RDG_rand(U32* src)
{
    static const U32 prime1 = 2654435761U;
    static const U32 prime2 = 2246822519U;
    U32 rand32 = *src;
    rand32 *= prime1;
    rand32 ^= prime2;
    rand32  = RDG_rotl32(rand32, 13);
    *src = rand32;
    return rand32 >> 5;
}


static void RDG_fillLiteralDistrib(BYTE* ldt, double ld)
{
    BYTE const firstChar = (ld<=0.0) ?   0 : '(';
    BYTE const lastChar  = (ld<=0.0) ? 255 : '}';
    BYTE character = (ld<=0.0) ? 0 : '0';
    U32 u;

    if (ld<=0.0) ld = 0.0;
    //TRACE(" percent:%5.2f%% \n", ld*100.);
    //TRACE(" start:(%c)[%02X] ", character, character);
    for (u=0; u<LTSIZE; ) {
        U32 const weight = (U32)((double)(LTSIZE - u) * ld) + 1;
        U32 const end = MIN ( u + weight , LTSIZE);
        while (u < end) ldt[u++] = character; // TRACE(" %u(%c)[%02X] ", u, character, character);
        character++;
        if (character > lastChar) character = firstChar;
    }
}


static BYTE RDG_genChar(U32* seed, const BYTE* ldt)
{
    U32 const id = RDG_rand(seed) & LTMASK;
    //TRACE(" %u : \n", id);
    //TRACE(" %4u [%4u] ; val : %4u \n", id, id&255, ldt[id]);
    return (ldt[id]);  /* memory-sanitizer fails here, stating "uninitialized value" when table initialized with 0.0. Checked : table is fully initialized */
}


#define RDG_RAND15BITS  ( RDG_rand(seed) & 0x7FFF )
#define RDG_RANDLENGTH  ( (RDG_rand(seed) & 7) ? (RDG_rand(seed) & 0xF) : (RDG_rand(seed) & 0x1FF) + 0xF)
void RDG_genBlock(void* buffer, size_t buffSize, size_t prefixSize, double matchProba, const BYTE* ldt, unsigned* seedPtr)
{
    BYTE* buffPtr = (BYTE*)buffer;
    const U32 matchProba32 = (U32)(32768 * matchProba);
    size_t pos = prefixSize;
    U32* seed = seedPtr;
    U32 prevOffset = 1;

    /* special case : sparse content */
    while (matchProba >= 1.0) {
        size_t size0 = RDG_rand(seed) & 3;
        size0  = (size_t)1 << (16 + size0 * 2);
        size0 += RDG_rand(seed) & (size0-1);   /* because size0 is power of 2*/
        if (buffSize < pos + size0) {
            memset(buffPtr+pos, 0, buffSize-pos);
            return;
        }
        memset(buffPtr+pos, 0, size0);
        pos += size0;
        buffPtr[pos-1] = RDG_genChar(seed, ldt);
        continue;
    }

    /* init */
    if (pos==0) buffPtr[0] = RDG_genChar(seed, ldt), pos=1;

    /* Generate compressible data */
    while (pos < buffSize) {
        /* Select : Literal (char) or Match (within 32K) */
        if (RDG_RAND15BITS < matchProba32) {
            /* Copy (within 32K) */
            U32 const length = RDG_RANDLENGTH + 4;
            U32 const d = MIN (pos + length , buffSize);
            U32 const repeatOffset = (RDG_rand(seed) & 15) == 2;
            U32 const randOffset = RDG_RAND15BITS + 1;
            U32 const offset = repeatOffset ? prevOffset : MIN(randOffset , pos);
            size_t match = pos - offset;
            //TRACE("pos : %u; offset: %u ; length : %u \n", (U32)pos, offset, length);
            while (pos < d) buffPtr[pos++] = buffPtr[match++];   /* correctly manages overlaps */
            prevOffset = offset;
        } else {
            /* Literal (noise) */
            U32 const length = RDG_RANDLENGTH;
            U32 const d = MIN(pos + length, buffSize);
            while (pos < d) buffPtr[pos++] = RDG_genChar(seed, ldt);
    }   }
}


void RDG_genBuffer(void* buffer, size_t size, double matchProba, double litProba, unsigned seed)
{
    BYTE ldt[LTSIZE];
    memset(ldt, '0', sizeof(ldt));
    if (litProba<=0.0) litProba = matchProba / 4.5;
    //TRACE(" percent:%5.2f%% \n", litProba*100.);
    RDG_fillLiteralDistrib(ldt, litProba);
    RDG_genBlock(buffer, size, 0, matchProba, ldt, &seed);
}


void RDG_genStdout(unsigned long long size, double matchProba, double litProba, unsigned seed)
{
    size_t const stdBlockSize = 128 KB;
    size_t const stdDictSize = 32 KB;
    BYTE* buff = (BYTE*)malloc(stdDictSize + stdBlockSize);
    U64 total = 0;
    BYTE ldt[LTSIZE];

    /* init */
    if (buff==NULL) { fprintf(stdout, "not enough memory\n"); exit(1); }
    if (litProba<=0.0) litProba = matchProba / 4.5;
    memset(ldt, '0', sizeof(ldt));
    RDG_fillLiteralDistrib(ldt, litProba);
    SET_BINARY_MODE(stdout);

    /* Generate initial dict */
    RDG_genBlock(buff, stdDictSize, 0, matchProba, ldt, &seed);

    /* Generate compressible data */
    while (total < size) {
        size_t const genBlockSize = (size_t) (MIN (stdBlockSize, size-total));
        RDG_genBlock(buff, stdDictSize+stdBlockSize, stdDictSize, matchProba, ldt, &seed);
        total += genBlockSize;
        { size_t const unused = fwrite(buff, 1, genBlockSize, stdout); (void)unused; }
        /* update dict */
        memcpy(buff, buff + stdBlockSize, stdDictSize);
    }

    /* cleanup */
    free(buff);
}
