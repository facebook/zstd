/*
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef FUNCTION
#  error "FUNCTION(name) must be defined"
#endif

#ifndef TARGET
#  error "TARGET must be defined"
#endif


static void FUNCTION(HUF_encodeSymbol)(BIT_CStream_t* bitCPtr, U32 symbol, const HUF_CElt* CTable)
{
    BIT_addBitsFast(bitCPtr, CTable[symbol].val, CTable[symbol].nbBits);
}

#define HUF_FLUSHBITS(s)  BIT_flushBits(s)

#define HUF_FLUSHBITS_1(stream) \
    if (sizeof((stream)->bitContainer)*8 < HUF_TABLELOG_MAX*2+7) HUF_FLUSHBITS(stream)

#define HUF_FLUSHBITS_2(stream) \
    if (sizeof((stream)->bitContainer)*8 < HUF_TABLELOG_MAX*4+7) HUF_FLUSHBITS(stream)

static TARGET
size_t FUNCTION(HUF_compress1X_usingCTable_internal)(void* dst, size_t dstSize, const void* src, size_t srcSize, const HUF_CElt* CTable)
{
    const BYTE* ip = (const BYTE*) src;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* const oend = ostart + dstSize;
    BYTE* op = ostart;
    size_t n;
    BIT_CStream_t bitC;

    /* init */
    if (dstSize < 8) return 0;   /* not enough space to compress */
    { size_t const initErr = BIT_initCStream(&bitC, op, oend-op);
      if (HUF_isError(initErr)) return 0; }

    n = srcSize & ~3;  /* join to mod 4 */
    switch (srcSize & 3)
    {
        case 3 : FUNCTION(HUF_encodeSymbol)(&bitC, ip[n+ 2], CTable);
                 HUF_FLUSHBITS_2(&bitC);
		 /* fall-through */
        case 2 : FUNCTION(HUF_encodeSymbol)(&bitC, ip[n+ 1], CTable);
                 HUF_FLUSHBITS_1(&bitC);
		 /* fall-through */
        case 1 : FUNCTION(HUF_encodeSymbol)(&bitC, ip[n+ 0], CTable);
                 HUF_FLUSHBITS(&bitC);
		 /* fall-through */
        case 0 : /* fall-through */
        default: break;
    }

    for (; n>0; n-=4) {  /* note : n&3==0 at this stage */
        FUNCTION(HUF_encodeSymbol)(&bitC, ip[n- 1], CTable);
        HUF_FLUSHBITS_1(&bitC);
        FUNCTION(HUF_encodeSymbol)(&bitC, ip[n- 2], CTable);
        HUF_FLUSHBITS_2(&bitC);
        FUNCTION(HUF_encodeSymbol)(&bitC, ip[n- 3], CTable);
        HUF_FLUSHBITS_1(&bitC);
        FUNCTION(HUF_encodeSymbol)(&bitC, ip[n- 4], CTable);
        HUF_FLUSHBITS(&bitC);
    }

    return BIT_closeCStream(&bitC);
}


static TARGET
size_t FUNCTION(HUF_compress4X_usingCTable_internal)(void* dst, size_t dstSize, const void* src, size_t srcSize, const HUF_CElt* CTable)
{
    size_t const segmentSize = (srcSize+3)/4;   /* first 3 segments */
    const BYTE* ip = (const BYTE*) src;
    const BYTE* const iend = ip + srcSize;
    BYTE* const ostart = (BYTE*) dst;
    BYTE* const oend = ostart + dstSize;
    BYTE* op = ostart;

    if (dstSize < 6 + 1 + 1 + 1 + 8) return 0;   /* minimum space to compress successfully */
    if (srcSize < 12) return 0;   /* no saving possible : too small input */
    op += 6;   /* jumpTable */

    {   CHECK_V_F(cSize, FUNCTION(HUF_compress1X_usingCTable_internal)(op, oend-op, ip, segmentSize, CTable) );
        if (cSize==0) return 0;
        MEM_writeLE16(ostart, (U16)cSize);
        op += cSize;
    }

    ip += segmentSize;
    {   CHECK_V_F(cSize, FUNCTION(HUF_compress1X_usingCTable_internal)(op, oend-op, ip, segmentSize, CTable) );
        if (cSize==0) return 0;
        MEM_writeLE16(ostart+2, (U16)cSize);
        op += cSize;
    }

    ip += segmentSize;
    {   CHECK_V_F(cSize, FUNCTION(HUF_compress1X_usingCTable_internal)(op, oend-op, ip, segmentSize, CTable) );
        if (cSize==0) return 0;
        MEM_writeLE16(ostart+4, (U16)cSize);
        op += cSize;
    }

    ip += segmentSize;
    {   CHECK_V_F(cSize, FUNCTION(HUF_compress1X_usingCTable_internal)(op, oend-op, ip, iend-ip, CTable) );
        if (cSize==0) return 0;
        op += cSize;
    }

    return op-ostart;
}
