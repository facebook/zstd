#include "zstdmt_decompress.h"
#include "compiler.h"
#include "cpu.h"
#include "fse.h"
#include "huf.h"
#include "mem.h"
#include "zstd_ddict.h"
#include "zstd_decompress_block.h"
#include "zstd_decompress_internal.h"
#include "zstd_internal.h"
#include "zstdmt_depthreadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-*******************************************************
 *  Decode Block Header
 *********************************************************/

typedef struct {
  int lastBlock;
  int blockType;
  size_t blockSize;
  const void *src;
  size_t srcSize;
  size_t headerSize;
} ZSTDMT_decodeBlockHeaderCtx;

static ZSTDMT_decodeBlockHeaderCtx *ZSTDMT_createDecodeBlockHeaderCtx() {
  ZSTDMT_decodeBlockHeaderCtx *ctx =
      malloc(sizeof(ZSTDMT_decodeBlockHeaderCtx));
  return ctx;
}

static void ZSTDMT_freeDecodeBlockHeaderCtx(ZSTDMT_decodeBlockHeaderCtx *ctx) {
  free(ctx);
}

static void ZSTDMT_decodeBlockHeader(void *data) {
  ZSTDMT_decodeBlockHeaderCtx *ctx = (ZSTDMT_decodeBlockHeaderCtx *)data;
  BYTE *src = (BYTE *)ctx->src;
  ctx->lastBlock = src[0] & 1;
  ctx->blockType = (src[0] >> 1) & 0b11;
  ctx->blockSize = MEM_readLE24(src) >> 3;
  ctx->headerSize = 3;
}

/*-*******************************************************
 *  Decode Literals Header
 *********************************************************/

typedef struct {
  int treeless;
  int singleStream;
  size_t rSize;
  size_t cSize;
  const void *src;
  size_t srcSize;
  size_t headerSize;
  int literalsBlockType;
  int sizeFormat;
} ZSTDMT_decodeLiteralsHeaderCtx;

static ZSTDMT_decodeLiteralsHeaderCtx *ZSTDMT_createDecodeLiteralsHeaderCtx() {
  ZSTDMT_decodeLiteralsHeaderCtx *ctx =
      malloc(sizeof(ZSTDMT_decodeLiteralsHeaderCtx));
  return ctx;
}

static void
ZSTDMT_freeDecodeLiteralsHeaderCtx(ZSTDMT_decodeLiteralsHeaderCtx *ctx) {
  free(ctx);
}

static void ZSTDMT_decodeLiteralsHeader(void *data) {
  ZSTDMT_decodeLiteralsHeaderCtx *ctx = (ZSTDMT_decodeLiteralsHeaderCtx *)data;
  BYTE *src = (BYTE *)ctx->src;
  U32 lhc;
  ctx->literalsBlockType = src[0] & 0b11;
  ctx->sizeFormat = (src[0] >> 2) & 0b11;
  ctx->treeless = 0;
  if (ctx->literalsBlockType == 0 || ctx->literalsBlockType == 1) {
    switch (ctx->sizeFormat) {
    case 0:
    case 2:
      ctx->headerSize = 1;
      ctx->rSize = src[0] >> 3;
      break;
    case 1:
      ctx->headerSize = 2;
      ctx->rSize = MEM_readLE16(src) >> 4;
      break;
    case 3:
      ctx->headerSize = 3;
      ctx->rSize = MEM_readLE24(src) >> 4;
      break;
    }
  } else {
    lhc = MEM_readLE32(src);
    if (ctx->literalsBlockType == 3)
      ctx->treeless = 1;
    switch (ctx->sizeFormat) {
    case 0:
    case 1:
      ctx->singleStream = !ctx->sizeFormat;
      ctx->headerSize = 3;
      ctx->rSize = (lhc >> 4) & 0x3FF;
      ctx->cSize = (lhc >> 14) & 0x3FF;
      break;
    case 2:
      ctx->headerSize = 4;
      ctx->rSize = (lhc >> 4) & 0x3FFF;
      ctx->cSize = lhc >> 18;
      break;
    case 3:
      ctx->headerSize = 5;
      ctx->rSize = (lhc >> 4) & 0x3FFFF;
      ctx->cSize = (lhc >> 22) + ((size_t)src[4] << 10);
      break;
    }
  }
}

/*-*******************************************************
 *  Decode Literals
 *********************************************************/

typedef struct {
  int singleStream;
  HUF_DTable *hufTable;
  BYTE litBuffer[ZSTD_BLOCKSIZE_MAX];
  size_t litBufferSize;
  const void *srcBuffer;
  size_t srcBufferSize;
  U32 workspace[HUF_DECOMPRESS_WORKSPACE_SIZE_U32];
  int bmi2;
  HUF_DTable *prevHufTable;
  int literalsBlockType;
} ZSTDMT_decodeLiteralsCtx;

static ZSTDMT_decodeLiteralsCtx *ZSTDMT_createDecodeLiteralsCtx() {
  ZSTDMT_decodeLiteralsCtx *ctx = malloc(sizeof(ZSTDMT_decodeLiteralsCtx));
  return ctx;
}

static void ZSTDMT_freeDecodeLiteralsCtx(ZSTDMT_decodeLiteralsCtx *ctx) {
  free(ctx);
}

static void ZSTDMT_decodeLiterals(void *data) {
  ZSTDMT_decodeLiteralsCtx *ctx = (ZSTDMT_decodeLiteralsCtx *)data;
  switch (ctx->literalsBlockType) {
  case 0:
    memcpy(ctx->litBuffer, ctx->srcBuffer, ctx->srcBufferSize);
    break;
  case 1:
    memset(ctx->litBuffer, *(BYTE *)ctx->srcBuffer, ctx->litBufferSize);
    break;
  default:
    if (ctx->prevHufTable != NULL) {
      if (ctx->singleStream) {
        HUF_decompress1X_usingDTable_bmi2(ctx->litBuffer, ctx->litBufferSize,
                                          ctx->srcBuffer, ctx->srcBufferSize,
                                          ctx->prevHufTable, ctx->bmi2);
      } else {
        HUF_decompress4X_usingDTable_bmi2(ctx->litBuffer, ctx->litBufferSize,
                                          ctx->srcBuffer, ctx->srcBufferSize,
                                          ctx->prevHufTable, ctx->bmi2);
      }
    } else {
      if (ctx->singleStream) {
        HUF_decompress1X1_DCtx_wksp_bmi2(ctx->hufTable, ctx->litBuffer,
                                         ctx->litBufferSize, ctx->srcBuffer,
                                         ctx->srcBufferSize, ctx->workspace,
                                         sizeof(ctx->workspace), ctx->bmi2);
      } else {
        HUF_decompress4X_hufOnly_wksp_bmi2(ctx->hufTable, ctx->litBuffer,
                                           ctx->litBufferSize, ctx->srcBuffer,
                                           ctx->srcBufferSize, ctx->workspace,
                                           sizeof(ctx->workspace), ctx->bmi2);
      }
    }
    break;
  }
}

/*-*******************************************************
 *  Decode Sequences
 *********************************************************/

typedef enum {
  ZSTD_lo_isRegularOffset,
  ZSTD_lo_isLongOffset = 1
} ZSTD_longOffset_e;

typedef struct {
  size_t litLength;
  size_t matchLength;
  size_t offset;
  const BYTE *match;
  int rep;
} seq_t;

typedef struct {
  size_t state;
  const ZSTD_seqSymbol *table;
} ZSTD_fseState;

typedef struct {
  BIT_DStream_t DStream;
  ZSTD_fseState stateLL;
  ZSTD_fseState stateOffb;
  ZSTD_fseState stateML;
  size_t prevOffset[ZSTD_REP_NUM];
  const BYTE *prefixStart;
  const BYTE *dictEnd;
  size_t pos;
} seqState_t;

#define LONG_OFFSETS_MAX_EXTRA_BITS_32                                         \
  (ZSTD_WINDOWLOG_MAX_32 > STREAM_ACCUMULATOR_MIN_32                           \
       ? ZSTD_WINDOWLOG_MAX_32 - STREAM_ACCUMULATOR_MIN_32                     \
       : 0)

static void ZSTD_initFseState(ZSTD_fseState *DStatePtr, BIT_DStream_t *bitD,
                              const ZSTD_seqSymbol *dt) {
  const void *ptr = dt;
  const ZSTD_seqSymbol_header *const DTableH =
      (const ZSTD_seqSymbol_header *)ptr;
  DStatePtr->state = BIT_readBits(bitD, DTableH->tableLog);
  BIT_reloadDStream(bitD);
  DStatePtr->table = dt + 1;
}

static void ZSTD_updateFseState(ZSTD_fseState *DStatePtr, BIT_DStream_t *bitD) {
  ZSTD_seqSymbol const DInfo = DStatePtr->table[DStatePtr->state];
  U32 const nbBits = DInfo.nbBits;
  size_t const lowBits = BIT_readBits(bitD, nbBits);
  DStatePtr->state = DInfo.nextState + lowBits;
}

static seq_t ZSTD_decodeSequence(seqState_t *seqState,
                                 const ZSTD_longOffset_e longOffsets) {
  seq_t seq;
  U32 const llBits =
      seqState->stateLL.table[seqState->stateLL.state].nbAdditionalBits;
  U32 const mlBits =
      seqState->stateML.table[seqState->stateML.state].nbAdditionalBits;
  U32 const ofBits =
      seqState->stateOffb.table[seqState->stateOffb.state].nbAdditionalBits;
  U32 const totalBits = llBits + mlBits + ofBits;
  U32 const llBase = seqState->stateLL.table[seqState->stateLL.state].baseValue;
  U32 const mlBase = seqState->stateML.table[seqState->stateML.state].baseValue;
  U32 const ofBase =
      seqState->stateOffb.table[seqState->stateOffb.state].baseValue;
  {
    size_t offset;
    if (!ofBits)
      offset = 0;
    else {
      assert(ofBits <= MaxOff);
      if (MEM_32bits() && longOffsets &&
          (ofBits >= STREAM_ACCUMULATOR_MIN_32)) {
        U32 const extraBits =
            ofBits - MIN(ofBits, 32 - seqState->DStream.bitsConsumed);
        offset =
            ofBase + (BIT_readBitsFast(&seqState->DStream, ofBits - extraBits)
                      << extraBits);
        BIT_reloadDStream(&seqState->DStream);
        if (extraBits)
          offset += BIT_readBitsFast(&seqState->DStream, extraBits);
        assert(extraBits <= LONG_OFFSETS_MAX_EXTRA_BITS_32);
      } else {
        offset = ofBase + BIT_readBitsFast(&seqState->DStream, ofBits);
        if (MEM_32bits())
          BIT_reloadDStream(&seqState->DStream);
      }
    }
    if (ofBits <= 1) {
      offset += (llBase == 0);
      if (offset) {
        size_t temp = (offset == 3) ? seqState->prevOffset[0] - 1
                                    : seqState->prevOffset[offset];
        temp += !temp;
        if (offset != 1)
          seqState->prevOffset[2] = seqState->prevOffset[1];
        seqState->prevOffset[1] = seqState->prevOffset[0];
        seqState->prevOffset[0] = offset = temp;
      } else {
        offset = seqState->prevOffset[0];
      }
    } else {
      seqState->prevOffset[2] = seqState->prevOffset[1];
      seqState->prevOffset[1] = seqState->prevOffset[0];
      seqState->prevOffset[0] = offset;
    }
    seq.offset = offset;
  }

  seq.matchLength =
      mlBase +
      ((mlBits > 0) ? BIT_readBitsFast(&seqState->DStream, mlBits) : 0);
  if (MEM_32bits() && (mlBits + llBits >= STREAM_ACCUMULATOR_MIN_32 -
                                              LONG_OFFSETS_MAX_EXTRA_BITS_32))
    BIT_reloadDStream(&seqState->DStream);
  if (MEM_64bits() && (totalBits >= STREAM_ACCUMULATOR_MIN_64 -
                                        (LLFSELog + MLFSELog + OffFSELog)))
    BIT_reloadDStream(&seqState->DStream);
  ZSTD_STATIC_ASSERT(16 + LLFSELog + MLFSELog + OffFSELog <
                     STREAM_ACCUMULATOR_MIN_64);

  seq.litLength =
      llBase +
      ((llBits > 0) ? BIT_readBitsFast(&seqState->DStream, llBits) : 0);
  if (MEM_32bits())
    BIT_reloadDStream(&seqState->DStream);

  ZSTD_updateFseState(&seqState->stateLL, &seqState->DStream);
  ZSTD_updateFseState(&seqState->stateML, &seqState->DStream);
  if (MEM_32bits())
    BIT_reloadDStream(&seqState->DStream);
  ZSTD_updateFseState(&seqState->stateOffb, &seqState->DStream);

  return seq;
}

typedef struct {
  const void *seqStart;
  size_t seqSize;
  int nbSeq;
  ZSTD_longOffset_e isLongOffset;
  ZSTD_entropyDTables_t *entropy;
  const ZSTD_seqSymbol *LLDTablePtr;
  const ZSTD_seqSymbol *OFDTablePtr;
  const ZSTD_seqSymbol *MLDTablePtr;
  seq_t *outSeqs;
  size_t outSeqsSize;
  seqState_t *lastUsedSeqState;
  seqState_t *prevSeqState;
} ZSTDMT_decodeSequencesCtx;

static ZSTDMT_decodeSequencesCtx *
ZSTDMT_createDecodeSequencesCtx(size_t outSeqsSize) {
  ZSTDMT_decodeSequencesCtx *ctx = malloc(sizeof(ZSTDMT_decodeSequencesCtx));
  ctx->outSeqs = malloc(outSeqsSize * sizeof(seq_t));
  ctx->lastUsedSeqState = malloc(sizeof(seqState_t));
  return ctx;
}

static void ZSTDMT_freeDecodeSequencesCtx(ZSTDMT_decodeSequencesCtx *ctx) {
  free(ctx->outSeqs);
  free(ctx->lastUsedSeqState);
  free(ctx);
}

static void ZSTDMT_decodeSequences(void *data) {
  ZSTDMT_decodeSequencesCtx *ctx = (ZSTDMT_decodeSequencesCtx *)data;
  const BYTE *ip = (const BYTE *)ctx->seqStart;
  const BYTE *const iend = ip + ctx->seqSize;
  if (ctx->nbSeq) {
    ctx->outSeqsSize = ctx->nbSeq;
    seqState_t seqState;
    size_t i;
    if (ctx->prevSeqState == NULL) {
      for (i = 0; i < ZSTD_REP_NUM; i++)
        seqState.prevOffset[i] = ctx->entropy->rep[i];
    } else {
      for (i = 0; i < ZSTD_REP_NUM; i++)
        seqState.prevOffset[i] = ctx->prevSeqState->prevOffset[i];
    }
    BIT_initDStream(&seqState.DStream, ip, iend - ip);
    ZSTD_initFseState(&seqState.stateLL, &seqState.DStream, ctx->LLDTablePtr);
    ZSTD_initFseState(&seqState.stateOffb, &seqState.DStream, ctx->OFDTablePtr);
    ZSTD_initFseState(&seqState.stateML, &seqState.DStream, ctx->MLDTablePtr);
    for (i = 0;
         (BIT_reloadDStream(&(seqState.DStream)) <= BIT_DStream_completed) &&
         ctx->nbSeq;
         i++) {
      ctx->nbSeq--;
      seq_t const sequence = ZSTD_decodeSequence(&seqState, ctx->isLongOffset);
      memmove(&ctx->outSeqs[i], &sequence, sizeof(sequence));
    }
    memmove(ctx->lastUsedSeqState, &seqState, sizeof(seqState));
  }
}

/*-*******************************************************
 *  Decode Sequences Header
 *********************************************************/

 static const ZSTD_seqSymbol LL_defaultDTable[(1<<LL_DEFAULTNORMLOG)+1] = {
      {  1,  1,  1, LL_DEFAULTNORMLOG},  /* header : fastMode, tableLog */
      /* nextState, nbAddBits, nbBits, baseVal */
      {  0,  0,  4,    0},  { 16,  0,  4,    0},
      { 32,  0,  5,    1},  {  0,  0,  5,    3},
      {  0,  0,  5,    4},  {  0,  0,  5,    6},
      {  0,  0,  5,    7},  {  0,  0,  5,    9},
      {  0,  0,  5,   10},  {  0,  0,  5,   12},
      {  0,  0,  6,   14},  {  0,  1,  5,   16},
      {  0,  1,  5,   20},  {  0,  1,  5,   22},
      {  0,  2,  5,   28},  {  0,  3,  5,   32},
      {  0,  4,  5,   48},  { 32,  6,  5,   64},
      {  0,  7,  5,  128},  {  0,  8,  6,  256},
      {  0, 10,  6, 1024},  {  0, 12,  6, 4096},
      { 32,  0,  4,    0},  {  0,  0,  4,    1},
      {  0,  0,  5,    2},  { 32,  0,  5,    4},
      {  0,  0,  5,    5},  { 32,  0,  5,    7},
      {  0,  0,  5,    8},  { 32,  0,  5,   10},
      {  0,  0,  5,   11},  {  0,  0,  6,   13},
      { 32,  1,  5,   16},  {  0,  1,  5,   18},
      { 32,  1,  5,   22},  {  0,  2,  5,   24},
      { 32,  3,  5,   32},  {  0,  3,  5,   40},
      {  0,  6,  4,   64},  { 16,  6,  4,   64},
      { 32,  7,  5,  128},  {  0,  9,  6,  512},
      {  0, 11,  6, 2048},  { 48,  0,  4,    0},
      { 16,  0,  4,    1},  { 32,  0,  5,    2},
      { 32,  0,  5,    3},  { 32,  0,  5,    5},
      { 32,  0,  5,    6},  { 32,  0,  5,    8},
      { 32,  0,  5,    9},  { 32,  0,  5,   11},
      { 32,  0,  5,   12},  {  0,  0,  6,   15},
      { 32,  1,  5,   18},  { 32,  1,  5,   20},
      { 32,  2,  5,   24},  { 32,  2,  5,   28},
      { 32,  3,  5,   40},  { 32,  4,  5,   48},
      {  0, 16,  6,65536},  {  0, 15,  6,32768},
      {  0, 14,  6,16384},  {  0, 13,  6, 8192},
 };   /* LL_defaultDTable */

 /* Default FSE distribution table for Offset Codes */
 static const ZSTD_seqSymbol OF_defaultDTable[(1<<OF_DEFAULTNORMLOG)+1] = {
     {  1,  1,  1, OF_DEFAULTNORMLOG},  /* header : fastMode, tableLog */
     /* nextState, nbAddBits, nbBits, baseVal */
     {  0,  0,  5,    0},     {  0,  6,  4,   61},
     {  0,  9,  5,  509},     {  0, 15,  5,32765},
     {  0, 21,  5,2097149},   {  0,  3,  5,    5},
     {  0,  7,  4,  125},     {  0, 12,  5, 4093},
     {  0, 18,  5,262141},    {  0, 23,  5,8388605},
     {  0,  5,  5,   29},     {  0,  8,  4,  253},
     {  0, 14,  5,16381},     {  0, 20,  5,1048573},
     {  0,  2,  5,    1},     { 16,  7,  4,  125},
     {  0, 11,  5, 2045},     {  0, 17,  5,131069},
     {  0, 22,  5,4194301},   {  0,  4,  5,   13},
     { 16,  8,  4,  253},     {  0, 13,  5, 8189},
     {  0, 19,  5,524285},    {  0,  1,  5,    1},
     { 16,  6,  4,   61},     {  0, 10,  5, 1021},
     {  0, 16,  5,65533},     {  0, 28,  5,268435453},
     {  0, 27,  5,134217725}, {  0, 26,  5,67108861},
     {  0, 25,  5,33554429},  {  0, 24,  5,16777213},
 };   /* OF_defaultDTable */


 /* Default FSE distribution table for Match Lengths */
 static const ZSTD_seqSymbol ML_defaultDTable[(1<<ML_DEFAULTNORMLOG)+1] = {
     {  1,  1,  1, ML_DEFAULTNORMLOG},  /* header : fastMode, tableLog */
     /* nextState, nbAddBits, nbBits, baseVal */
     {  0,  0,  6,    3},  {  0,  0,  4,    4},
     { 32,  0,  5,    5},  {  0,  0,  5,    6},
     {  0,  0,  5,    8},  {  0,  0,  5,    9},
     {  0,  0,  5,   11},  {  0,  0,  6,   13},
     {  0,  0,  6,   16},  {  0,  0,  6,   19},
     {  0,  0,  6,   22},  {  0,  0,  6,   25},
     {  0,  0,  6,   28},  {  0,  0,  6,   31},
     {  0,  0,  6,   34},  {  0,  1,  6,   37},
     {  0,  1,  6,   41},  {  0,  2,  6,   47},
     {  0,  3,  6,   59},  {  0,  4,  6,   83},
     {  0,  7,  6,  131},  {  0,  9,  6,  515},
     { 16,  0,  4,    4},  {  0,  0,  4,    5},
     { 32,  0,  5,    6},  {  0,  0,  5,    7},
     { 32,  0,  5,    9},  {  0,  0,  5,   10},
     {  0,  0,  6,   12},  {  0,  0,  6,   15},
     {  0,  0,  6,   18},  {  0,  0,  6,   21},
     {  0,  0,  6,   24},  {  0,  0,  6,   27},
     {  0,  0,  6,   30},  {  0,  0,  6,   33},
     {  0,  1,  6,   35},  {  0,  1,  6,   39},
     {  0,  2,  6,   43},  {  0,  3,  6,   51},
     {  0,  4,  6,   67},  {  0,  5,  6,   99},
     {  0,  8,  6,  259},  { 32,  0,  4,    4},
     { 48,  0,  4,    4},  { 16,  0,  4,    5},
     { 32,  0,  5,    7},  { 32,  0,  5,    8},
     { 32,  0,  5,   10},  { 32,  0,  5,   11},
     {  0,  0,  6,   14},  {  0,  0,  6,   17},
     {  0,  0,  6,   20},  {  0,  0,  6,   23},
     {  0,  0,  6,   26},  {  0,  0,  6,   29},
     {  0,  0,  6,   32},  {  0, 16,  6,65539},
     {  0, 15,  6,32771},  {  0, 14,  6,16387},
     {  0, 13,  6, 8195},  {  0, 12,  6, 4099},
     {  0, 11,  6, 2051},  {  0, 10,  6, 1027},
 };   /* ML_defaultDTable */

static ZSTD_entropyDTables_t *ZSTDMT_createEntropy() {
  ZSTD_entropyDTables_t *entropy = malloc(sizeof(ZSTD_entropyDTables_t));
  return entropy;
}

static void ZSTDMT_freeEntropy(ZSTD_entropyDTables_t *entropy) {
  free(entropy);
}

typedef struct {
  int firstBlock;
  int nbSeq;
  size_t headerSize;
  const void *src;
  size_t srcSize;
  ZSTD_longOffset_e isLongOffset;
  int repeat;
  ZSTD_entropyDTables_t *entropy;
  const ZSTD_seqSymbol *LLDTablePtr;
  const ZSTD_seqSymbol *OFDTablePtr;
  const ZSTD_seqSymbol *MLDTablePtr;
} ZSTDMT_decodeSequencesHeaderCtx;

static int ZSTDMT_isSequenceRepeat(const void *seqStart, size_t seqsSize,
                                   ZSTD_entropyDTables_t *entropy,
                                   const ZSTD_seqSymbol *LLDTablePtr,
                                   const ZSTD_seqSymbol *OFDTablePtr,
                                   const ZSTD_seqSymbol *MLDTablePtr,
                                   const ZSTD_longOffset_e isLongOffset) {
  seqState_t seqState;
  U32 i;
  for (i = 0; i < ZSTD_REP_NUM; i++)
    seqState.prevOffset[i] = entropy->rep[i];
  BIT_initDStream(&seqState.DStream, seqStart, seqsSize);
  ZSTD_initFseState(&seqState.stateLL, &seqState.DStream, LLDTablePtr);
  ZSTD_initFseState(&seqState.stateOffb, &seqState.DStream, OFDTablePtr);
  ZSTD_initFseState(&seqState.stateML, &seqState.DStream, MLDTablePtr);
  for (i = 0; (BIT_reloadDStream(&(seqState.DStream)) <= BIT_DStream_completed);
       i++) {
    seq_t sequence = ZSTD_decodeSequence(&seqState, isLongOffset);
    if (sequence.offset <= 3)
      return 1;
    if (i >= 2)
      break;
  }
  return 0;
}

static ZSTDMT_decodeSequencesHeaderCtx *
ZSTDMT_createDecodeSequencesHeaderCtx() {
  ZSTDMT_decodeSequencesHeaderCtx *ctx =
      malloc(sizeof(ZSTDMT_decodeSequencesHeaderCtx));
  ctx->entropy = ZSTDMT_createEntropy();
  return ctx;
}

static void
ZSTDMT_freeDecodeSequencesHeaderCtx(ZSTDMT_decodeSequencesHeaderCtx *ctx) {
  ZSTDMT_freeEntropy(ctx->entropy);
  free(ctx);
}

static void ZSTDMT_decodeSequencesHeader(void *data) {
  ZSTDMT_decodeSequencesHeaderCtx *ctx =
      (ZSTDMT_decodeSequencesHeaderCtx *)data;
  const BYTE *const istart = (const BYTE *const)ctx->src;
  const BYTE *const iend = istart + ctx->srcSize;
  const BYTE *ip = istart;
  int nbSeq;
  nbSeq = *ip++;
  if (!nbSeq) {
    ctx->nbSeq = 0;
    ctx->headerSize = 1;
    return;
  }
  if (nbSeq > 0x7F) {
    if (nbSeq == 0xFF) {
      nbSeq = MEM_readLE16(ip) + LONGNBSEQ, ip += 2;
    } else {
      nbSeq = ((nbSeq - 0x80) << 8) + *ip++;
    }
  }
  ctx->nbSeq = nbSeq;
  {
    symbolEncodingType_e const LLtype = (symbolEncodingType_e)(*ip >> 6);
    symbolEncodingType_e const OFtype = (symbolEncodingType_e)((*ip >> 4) & 3);
    symbolEncodingType_e const MLtype = (symbolEncodingType_e)((*ip >> 2) & 3);
    ip++;
    {
      size_t const llhSize = ZSTD_buildSeqTable(
          ctx->LLDTablePtr, &ctx->LLDTablePtr, LLtype, MaxLL, LLFSELog, ip,
          iend - ip, LL_base, LL_bits, LL_defaultDTable, 1, 0, nbSeq);
      ip += llhSize;
    }
    {
      size_t const ofhSize = ZSTD_buildSeqTable(
          ctx->OFDTablePtr, &ctx->OFDTablePtr, OFtype, MaxOff, OffFSELog, ip,
          iend - ip, OF_base, OF_bits, OF_defaultDTable, 1, 0, nbSeq);
      ip += ofhSize;
    }
    {
      size_t const mlhSize = ZSTD_buildSeqTable(
          ctx->MLDTablePtr, &ctx->MLDTablePtr, MLtype, MaxML, MLFSELog, ip,
          iend - ip, ML_base, ML_bits, ML_defaultDTable, 1, 0, nbSeq);
      ip += mlhSize;
    }
  }
  ctx->headerSize = ip - istart;
  ZSTD_entropyDTables_t entropy;
  ctx->repeat = !ctx->firstBlock
                    ? ZSTDMT_isSequenceRepeat(
                          ip, iend - ip, ctx->entropy, ctx->LLDTablePtr,
                          ctx->OFDTablePtr, ctx->MLDTablePtr, ctx->isLongOffset)
                    : 0;
}

/*-*******************************************************
 *  MT everything by LZ internal functions
 *********************************************************/

#define MAX_BLOCKS 500

typedef struct {
  ZSTDMT_decodeBlockHeaderCtx *blockHeaders[MAX_BLOCKS];
  ZSTDMT_decodeLiteralsHeaderCtx *literalsHeaders[MAX_BLOCKS];
  ZSTDMT_decodeLiteralsCtx *literals[MAX_BLOCKS];
  ZSTDMT_decodeSequencesHeaderCtx *sequencesHeaders[MAX_BLOCKS];
  ZSTDMT_decodeSequencesCtx *sequences[MAX_BLOCKS];
  size_t nbBlocks;
  size_t nbJobs;
} ZSTDMT_Ctx;

static void ZSTDMT_beginDecompress(ZSTDMT_Ctx *ctx, void *voidDst,
                                   size_t dstSize, const void *voidSrc,
                                   size_t srcSize) {
  BYTE *src = (BYTE *)voidSrc;
  BYTE *dst = (BYTE *)voidDst;
  src += ZSTD_frameHeaderSize(src, srcSize);
  size_t nbBlocks = 0;
  size_t nbJobs = 0;
  HUF_DTable *prevHufTable;
  seqState_t *prevSeqState;
  int firstBlock = 1;
  while (1) {
    ZSTDMT_decodeBlockHeaderCtx *blockHeaderCtx =
        ZSTDMT_createDecodeBlockHeaderCtx();
    blockHeaderCtx->src = src;
    blockHeaderCtx->srcSize = srcSize;
    ZSTDMT_decodeBlockHeader((void *)blockHeaderCtx);
    ctx->blockHeaders[nbBlocks] = blockHeaderCtx;

    if (blockHeaderCtx->blockType <= 1) {
      src += blockHeaderCtx->headerSize +
             (blockHeaderCtx->blockType == 0 ? blockHeaderCtx->blockSize : 1);
      nbBlocks++;
      firstBlock = 0;
      if (blockHeaderCtx->lastBlock)
        break;
      continue;
    }

    ZSTDMT_decodeLiteralsHeaderCtx *literalsHeaderCtx =
        ZSTDMT_createDecodeLiteralsHeaderCtx();
    literalsHeaderCtx->src = src + blockHeaderCtx->headerSize;
    literalsHeaderCtx->srcSize = srcSize - blockHeaderCtx->headerSize;
    ZSTDMT_decodeLiteralsHeader((void *)literalsHeaderCtx);
    ctx->literalsHeaders[nbBlocks] = literalsHeaderCtx;

    ZSTD_entropyDTables_t *entropy = ZSTDMT_createEntropy();
    memcpy(entropy->rep, repStartValue, sizeof(repStartValue));
    ZSTDMT_decodeLiteralsCtx *literalsCtx = ZSTDMT_createDecodeLiteralsCtx();
    literalsCtx->singleStream = 0;
    literalsCtx->literalsBlockType = literalsHeaderCtx->literalsBlockType;
    literalsCtx->litBufferSize = literalsHeaderCtx->rSize;
    literalsCtx->srcBuffer =
        literalsHeaderCtx->src + literalsHeaderCtx->headerSize;
    if (literalsCtx->literalsBlockType <= 1) {
      literalsCtx->srcBufferSize =
          literalsCtx->literalsBlockType == 0 ? literalsCtx->litBufferSize : 1;
    } else {
      literalsCtx->srcBufferSize = literalsHeaderCtx->cSize;
      literalsCtx->hufTable = entropy->hufTable;
      literalsCtx->hufTable[0] = (HUF_DTable)((HufLog)*0x1000001);
      literalsCtx->bmi2 = ZSTD_cpuid_bmi2(ZSTD_cpuid());
      if (literalsHeaderCtx->treeless) {
        literalsCtx->prevHufTable = prevHufTable;
      } else {
        literalsCtx->prevHufTable = NULL;
        prevHufTable = literalsCtx->hufTable;
      }
    }
    ctx->literals[nbBlocks] = literalsCtx;
    nbJobs++;

    ZSTDMT_decodeSequencesHeaderCtx *sequencesHeaderCtx =
        ZSTDMT_createDecodeSequencesHeaderCtx();
    sequencesHeaderCtx->entropy = entropy;
    sequencesHeaderCtx->src =
        literalsCtx->srcBuffer + literalsCtx->srcBufferSize;
    sequencesHeaderCtx->srcSize = blockHeaderCtx->blockSize -
                                  literalsCtx->srcBufferSize -
                                  literalsHeaderCtx->headerSize;
    sequencesHeaderCtx->LLDTablePtr = entropy->LLTable;
    sequencesHeaderCtx->OFDTablePtr = entropy->OFTable;
    sequencesHeaderCtx->MLDTablePtr = entropy->MLTable;
    sequencesHeaderCtx->isLongOffset = ZSTD_lo_isRegularOffset;
    sequencesHeaderCtx->firstBlock = firstBlock;
    ZSTDMT_decodeSequencesHeader((void *)sequencesHeaderCtx);
    ctx->sequencesHeaders[nbBlocks] = sequencesHeaderCtx;

    ZSTDMT_decodeSequencesCtx *sequencesCtx =
        ZSTDMT_createDecodeSequencesCtx(sequencesHeaderCtx->nbSeq);
    sequencesCtx->seqStart =
        sequencesHeaderCtx->src + sequencesHeaderCtx->headerSize;
    sequencesCtx->seqSize =
        blockHeaderCtx->blockSize - literalsCtx->srcBufferSize -
        literalsHeaderCtx->headerSize - sequencesHeaderCtx->headerSize;
    sequencesCtx->nbSeq = sequencesHeaderCtx->nbSeq;
    sequencesCtx->entropy = entropy;
    sequencesCtx->LLDTablePtr = sequencesHeaderCtx->LLDTablePtr;
    sequencesCtx->OFDTablePtr = sequencesHeaderCtx->OFDTablePtr;
    sequencesCtx->MLDTablePtr = sequencesHeaderCtx->MLDTablePtr;
    sequencesCtx->isLongOffset = ZSTD_lo_isRegularOffset;
    sequencesCtx->prevSeqState =
        sequencesHeaderCtx->repeat ? prevSeqState : NULL;
    prevSeqState = sequencesCtx->lastUsedSeqState;
    ctx->sequences[nbBlocks] = sequencesCtx;
    nbJobs++;

    src = (BYTE *)sequencesCtx->seqStart + sequencesCtx->seqSize;
    nbBlocks++;
    firstBlock = 0;
    if (blockHeaderCtx->lastBlock)
      break;
  }
  ctx->nbBlocks = nbBlocks;
  ctx->nbJobs = nbJobs;
}

#define NB_THREADS 4

static void ZSTDMT_middleDecompress(ZSTDMT_Ctx *ctx, void *voidDst,
                                    size_t dstSize, const void *voidSrc,
                                    size_t srcSize) {
  ZSTDMT_DepThreadPoolCtx *depThreadPoolCtx =
      ZSTDMT_depThreadPool_createCtx(ctx->nbJobs, NB_THREADS);
  BYTE *src = (BYTE *)voidSrc;
  BYTE *dst = (BYTE *)voidDst;
  src += ZSTD_frameHeaderSize(src, srcSize);
  size_t i;
  size_t jobId;
  size_t prevDecodeLiteralsHeaderJobId;
  size_t prevDecodeSequencesHeaderJobId;
  for (i = 0, jobId = 0; i < ctx->nbBlocks; ++i) {
    src += ctx->blockHeaders[i]->headerSize;
    if (ctx->blockHeaders[i]->blockType <= 1)
      continue;

    if (!ctx->literalsHeaders[i]->treeless) {
      ZSTDMT_depThreadPool_addJob(depThreadPoolCtx, ZSTDMT_decodeLiterals,
                                  (void *)ctx->literals[i], 0, NULL);
    } else {
      size_t depJobIds[] = {prevDecodeLiteralsHeaderJobId};
      ZSTDMT_depThreadPool_addJob(depThreadPoolCtx, ZSTDMT_decodeLiterals,
                                  (void *)ctx->literals[i], 1, depJobIds);
    }
    src +=
        ctx->literals[i]->srcBufferSize + ctx->literalsHeaders[i]->headerSize;
    prevDecodeLiteralsHeaderJobId = jobId;
    jobId++;

    if (!ctx->sequencesHeaders[i]->repeat) {
      size_t depJobIds[] = {jobId - 1};
      ZSTDMT_depThreadPool_addJob(depThreadPoolCtx, ZSTDMT_decodeSequences,
                                  (void *)ctx->sequences[i], 1, depJobIds);
    } else {
      size_t depJobIds[] = {prevDecodeSequencesHeaderJobId, jobId - 1};
      ZSTDMT_depThreadPool_addJob(depThreadPoolCtx, ZSTDMT_decodeSequences,
                                  (void *)ctx->sequences[i], 2, depJobIds);
    }
    src += ctx->sequences[i]->seqSize + ctx->sequencesHeaders[i]->headerSize;
    prevDecodeSequencesHeaderJobId = jobId;
    jobId++;
  }
  ZSTDMT_depThreadPool_destroyCtx(depThreadPoolCtx);
}

static void ZSTDMT_endDecompress(ZSTDMT_Ctx *ctx) {
  size_t i = 0;
  for (i = 0; i < ctx->nbBlocks; ++i) {
    if (ctx->blockHeaders[i]->blockType > 1) {
      ZSTDMT_freeDecodeLiteralsHeaderCtx(ctx->literalsHeaders[i]);
      ZSTDMT_freeDecodeLiteralsCtx(ctx->literals[i]);
      ZSTDMT_freeDecodeSequencesHeaderCtx(ctx->sequencesHeaders[i]);
      ZSTDMT_freeDecodeSequencesCtx(ctx->sequences[i]);
    }
    ZSTDMT_freeDecodeBlockHeaderCtx(ctx->blockHeaders[i]);
  }
}

/*-*******************************************************
 *  Apply LZ
 *********************************************************/

static size_t ZSTDMT_applyLZ(ZSTDMT_Ctx *ctx, void *voidDst, size_t dstSize) {
  BYTE *const ostart = (BYTE *const)voidDst;
  BYTE *const oend = ostart + dstSize;
  BYTE *op = ostart;
  seq_t *seqs;
  size_t i;
  size_t j;
  for (i = 0; i < ctx->nbBlocks; ++i) {
    switch (ctx->blockHeaders[i]->blockType) {
    case 0:
      memcpy(op, ctx->blockHeaders[i]->src + ctx->blockHeaders[i]->headerSize,
             ctx->blockHeaders[i]->blockSize);
      op += ctx->blockHeaders[i]->blockSize;
      break;
    case 1:
      memset(op,
             *((BYTE *)ctx->blockHeaders[i]->src +
               ctx->blockHeaders[i]->headerSize),
             ctx->blockHeaders[i]->blockSize);
      op += ctx->blockHeaders[i]->blockSize;
      break;
    default:
      seqs = ctx->sequences[i]->outSeqs;
      size_t seqsSize = ctx->sequences[i]->outSeqsSize;
      BYTE *litBuffer = ctx->literals[i]->litBuffer;
      size_t litBufferSize = ctx->literals[i]->litBufferSize;
      const BYTE *litEnd = litBuffer + litBufferSize;
      for (j = 0; j < seqsSize; ++j) {
        seq_t const sequence = seqs[j];
        size_t const oneSeqSize = ZSTD_execSequence(
            op, oend, sequence, &litBuffer, litEnd, voidDst, voidDst, 0);
        op += oneSeqSize;
      }
      size_t const lastLLSize = litEnd - litBuffer;
      memcpy(op, litBuffer, lastLLSize);
      op += lastLLSize;
    }
  }
  return op - ostart;
}

/*-*******************************************************
 *  ZSTDMT_decompress API
 *********************************************************/

size_t ZSTDMT_decompress(void *voidDst, size_t dstSize, const void *voidSrc,
                         size_t srcSize) {
  ZSTDMT_Ctx ctx;
  ZSTDMT_beginDecompress(&ctx, voidDst, dstSize, voidSrc, srcSize);
  ZSTDMT_middleDecompress(&ctx, voidDst, dstSize, voidSrc, srcSize);
  size_t dSize = ZSTDMT_applyLZ(&ctx, voidDst, dstSize);
  ZSTDMT_endDecompress(&ctx);
  return dSize;
}
