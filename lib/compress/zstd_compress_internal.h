/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* This header contains definitions
 * that shall **only** be used by modules within lib/compress.
 */

#ifndef ZSTD_COMPRESS_H
#define ZSTD_COMPRESS_H

/*-*************************************
*  Dependencies
***************************************/
#include "../common/zstd_internal.h"
#include "zstd_cwksp.h"
#ifdef ZSTD_MULTITHREAD
#  include "zstdmt_compress.h"
#endif
#include "../common/bits.h" /* ZSTD_highbit32, ZSTD_NbCommonBytes */
#include "zstd_preSplit.h" /* ZSTD_SLIPBLOCK_WORKSPACESIZE */

#if !defined(ZSTD_NO_INTRINSICS) && defined(ZSTD_HASH_USE_CRC32C)
/*
   32-bit builds with SSE 4.2 do not have _mm_crc32_u64, so the
   __x86_64__ condition is necessary.
*/
#if defined(__SSE4_2__) && defined(__x86_64__)

#include <nmmintrin.h>
#define ZSTD_HASH_INTERNAL_CRC32_U64 _mm_crc32_u64

#elif defined(_MSC_VER) && !defined(__clang__) && defined(__AVX__)

/* MSVC AVX (/arch:AVX) implies SSE 4.2. */
#include <nmmintrin.h>
#define ZSTD_HASH_INTERNAL_CRC32_U64 _mm_crc32_u64

#elif defined(__ARM_FEATURE_CRC32)

#include <arm_acle.h>
/* Casting to uint32_t to be consistent with x86 intrinsic (_mm_crc32_u64
   accepts crc as 64 bit integer).
*/
#define ZSTD_HASH_INTERNAL_CRC32_U64(crc, data) \
  __crc32cd((U32)(crc), data)

#else

HINT_INLINE U32 ZSTD_HASH_INTERNAL_CRC32_U64(U32 seed, U64 data) {
  static const U32 crc32_table[] = {
      0x00000000U, 0xf26b8303U, 0xe13b70f7U, 0x1350f3f4U, 0xc79a971fU,
      0x35f1141cU, 0x26a1e7e8U, 0xd4ca64ebU, 0x8ad958cfU, 0x78b2dbccU,
      0x6be22838U, 0x9989ab3bU, 0x4d43cfd0U, 0xbf284cd3U, 0xac78bf27U,
      0x5e133c24U, 0x105ec76fU, 0xe235446cU, 0xf165b798U, 0x030e349bU,
      0xd7c45070U, 0x25afd373U, 0x36ff2087U, 0xc494a384U, 0x9a879fa0U,
      0x68ec1ca3U, 0x7bbcef57U, 0x89d76c54U, 0x5d1d08bfU, 0xaf768bbcU,
      0xbc267848U, 0x4e4dfb4bU, 0x20bd8edeU, 0xd2d60dddU, 0xc186fe29U,
      0x33ed7d2aU, 0xe72719c1U, 0x154c9ac2U, 0x061c6936U, 0xf477ea35U,
      0xaa64d611U, 0x580f5512U, 0x4b5fa6e6U, 0xb93425e5U, 0x6dfe410eU,
      0x9f95c20dU, 0x8cc531f9U, 0x7eaeb2faU, 0x30e349b1U, 0xc288cab2U,
      0xd1d83946U, 0x23b3ba45U, 0xf779deaeU, 0x05125dadU, 0x1642ae59U,
      0xe4292d5aU, 0xba3a117eU, 0x4851927dU, 0x5b016189U, 0xa96ae28aU,
      0x7da08661U, 0x8fcb0562U, 0x9c9bf696U, 0x6ef07595U, 0x417b1dbcU,
      0xb3109ebfU, 0xa0406d4bU, 0x522bee48U, 0x86e18aa3U, 0x748a09a0U,
      0x67dafa54U, 0x95b17957U, 0xcba24573U, 0x39c9c670U, 0x2a993584U,
      0xd8f2b687U, 0x0c38d26cU, 0xfe53516fU, 0xed03a29bU, 0x1f682198U,
      0x5125dad3U, 0xa34e59d0U, 0xb01eaa24U, 0x42752927U, 0x96bf4dccU,
      0x64d4cecfU, 0x77843d3bU, 0x85efbe38U, 0xdbfc821cU, 0x2997011fU,
      0x3ac7f2ebU, 0xc8ac71e8U, 0x1c661503U, 0xee0d9600U, 0xfd5d65f4U,
      0x0f36e6f7U, 0x61c69362U, 0x93ad1061U, 0x80fde395U, 0x72966096U,
      0xa65c047dU, 0x5437877eU, 0x4767748aU, 0xb50cf789U, 0xeb1fcbadU,
      0x197448aeU, 0x0a24bb5aU, 0xf84f3859U, 0x2c855cb2U, 0xdeeedfb1U,
      0xcdbe2c45U, 0x3fd5af46U, 0x7198540dU, 0x83f3d70eU, 0x90a324faU,
      0x62c8a7f9U, 0xb602c312U, 0x44694011U, 0x5739b3e5U, 0xa55230e6U,
      0xfb410cc2U, 0x092a8fc1U, 0x1a7a7c35U, 0xe811ff36U, 0x3cdb9bddU,
      0xceb018deU, 0xdde0eb2aU, 0x2f8b6829U, 0x82f63b78U, 0x709db87bU,
      0x63cd4b8fU, 0x91a6c88cU, 0x456cac67U, 0xb7072f64U, 0xa457dc90U,
      0x563c5f93U, 0x082f63b7U, 0xfa44e0b4U, 0xe9141340U, 0x1b7f9043U,
      0xcfb5f4a8U, 0x3dde77abU, 0x2e8e845fU, 0xdce5075cU, 0x92a8fc17U,
      0x60c37f14U, 0x73938ce0U, 0x81f80fe3U, 0x55326b08U, 0xa759e80bU,
      0xb4091bffU, 0x466298fcU, 0x1871a4d8U, 0xea1a27dbU, 0xf94ad42fU,
      0x0b21572cU, 0xdfeb33c7U, 0x2d80b0c4U, 0x3ed04330U, 0xccbbc033U,
      0xa24bb5a6U, 0x502036a5U, 0x4370c551U, 0xb11b4652U, 0x65d122b9U,
      0x97baa1baU, 0x84ea524eU, 0x7681d14dU, 0x2892ed69U, 0xdaf96e6aU,
      0xc9a99d9eU, 0x3bc21e9dU, 0xef087a76U, 0x1d63f975U, 0x0e330a81U,
      0xfc588982U, 0xb21572c9U, 0x407ef1caU, 0x532e023eU, 0xa145813dU,
      0x758fe5d6U, 0x87e466d5U, 0x94b49521U, 0x66df1622U, 0x38cc2a06U,
      0xcaa7a905U, 0xd9f75af1U, 0x2b9cd9f2U, 0xff56bd19U, 0x0d3d3e1aU,
      0x1e6dcdeeU, 0xec064eedU, 0xc38d26c4U, 0x31e6a5c7U, 0x22b65633U,
      0xd0ddd530U, 0x0417b1dbU, 0xf67c32d8U, 0xe52cc12cU, 0x1747422fU,
      0x49547e0bU, 0xbb3ffd08U, 0xa86f0efcU, 0x5a048dffU, 0x8ecee914U,
      0x7ca56a17U, 0x6ff599e3U, 0x9d9e1ae0U, 0xd3d3e1abU, 0x21b862a8U,
      0x32e8915cU, 0xc083125fU, 0x144976b4U, 0xe622f5b7U, 0xf5720643U,
      0x07198540U, 0x590ab964U, 0xab613a67U, 0xb831c993U, 0x4a5a4a90U,
      0x9e902e7bU, 0x6cfbad78U, 0x7fab5e8cU, 0x8dc0dd8fU, 0xe330a81aU,
      0x115b2b19U, 0x020bd8edU, 0xf0605beeU, 0x24aa3f05U, 0xd6c1bc06U,
      0xc5914ff2U, 0x37faccf1U, 0x69e9f0d5U, 0x9b8273d6U, 0x88d28022U,
      0x7ab90321U, 0xae7367caU, 0x5c18e4c9U, 0x4f48173dU, 0xbd23943eU,
      0xf36e6f75U, 0x0105ec76U, 0x12551f82U, 0xe03e9c81U, 0x34f4f86aU,
      0xc69f7b69U, 0xd5cf889dU, 0x27a40b9eU, 0x79b737baU, 0x8bdcb4b9U,
      0x988c474dU, 0x6ae7c44eU, 0xbe2da0a5U, 0x4c4623a6U, 0x5f16d052U,
      0xad7d5351U,
  };

  U32 crc = seed;
  const char* p;
  int i;
  char c;
  p = (const char*)&data;
  for (i = 0; i < 8; ++i) {
    c = MEM_isLittleEndian() ? p[i] : p[7 - i];
    crc = crc32_table[(crc ^ c) & 0xff] ^ (crc >> 8);
  }

  return crc;
}

#endif

#endif


/*-*************************************
*  Constants
***************************************/
#define kSearchStrength      8
#define HASH_READ_SIZE       8
#define ZSTD_DUBT_UNSORTED_MARK 1   /* For btlazy2 strategy, index ZSTD_DUBT_UNSORTED_MARK==1 means "unsorted".
                                       It could be confused for a real successor at index "1", if sorted as larger than its predecessor.
                                       It's not a big deal though : candidate will just be sorted again.
                                       Additionally, candidate position 1 will be lost.
                                       But candidate 1 cannot hide a large tree of candidates, so it's a minimal loss.
                                       The benefit is that ZSTD_DUBT_UNSORTED_MARK cannot be mishandled after table reuse with a different strategy.
                                       This constant is required by ZSTD_compressBlock_btlazy2() and ZSTD_reduceTable_internal() */


/*-*************************************
*  Context memory management
***************************************/
typedef enum { ZSTDcs_created=0, ZSTDcs_init, ZSTDcs_ongoing, ZSTDcs_ending } ZSTD_compressionStage_e;
typedef enum { zcss_init=0, zcss_load, zcss_flush } ZSTD_cStreamStage;

typedef struct ZSTD_prefixDict_s {
    const void* dict;
    size_t dictSize;
    ZSTD_dictContentType_e dictContentType;
} ZSTD_prefixDict;

typedef struct {
    void* dictBuffer;
    void const* dict;
    size_t dictSize;
    ZSTD_dictContentType_e dictContentType;
    ZSTD_CDict* cdict;
} ZSTD_localDict;

typedef struct {
    HUF_CElt CTable[HUF_CTABLE_SIZE_ST(255)];
    HUF_repeat repeatMode;
} ZSTD_hufCTables_t;

typedef struct {
    FSE_CTable offcodeCTable[FSE_CTABLE_SIZE_U32(OffFSELog, MaxOff)];
    FSE_CTable matchlengthCTable[FSE_CTABLE_SIZE_U32(MLFSELog, MaxML)];
    FSE_CTable litlengthCTable[FSE_CTABLE_SIZE_U32(LLFSELog, MaxLL)];
    FSE_repeat offcode_repeatMode;
    FSE_repeat matchlength_repeatMode;
    FSE_repeat litlength_repeatMode;
} ZSTD_fseCTables_t;

typedef struct {
    ZSTD_hufCTables_t huf;
    ZSTD_fseCTables_t fse;
} ZSTD_entropyCTables_t;

/***********************************************
*  Sequences *
***********************************************/
typedef struct SeqDef_s {
    U32 offBase;   /* offBase == Offset + ZSTD_REP_NUM, or repcode 1,2,3 */
    U16 litLength;
    U16 mlBase;    /* mlBase == matchLength - MINMATCH */
} SeqDef;

/* Controls whether seqStore has a single "long" litLength or matchLength. See SeqStore_t. */
typedef enum {
    ZSTD_llt_none = 0,             /* no longLengthType */
    ZSTD_llt_literalLength = 1,    /* represents a long literal */
    ZSTD_llt_matchLength = 2       /* represents a long match */
} ZSTD_longLengthType_e;

typedef struct {
    SeqDef* sequencesStart;
    SeqDef* sequences;      /* ptr to end of sequences */
    BYTE*  litStart;
    BYTE*  lit;             /* ptr to end of literals */
    BYTE*  llCode;
    BYTE*  mlCode;
    BYTE*  ofCode;
    size_t maxNbSeq;
    size_t maxNbLit;

    /* longLengthPos and longLengthType to allow us to represent either a single litLength or matchLength
     * in the seqStore that has a value larger than U16 (if it exists). To do so, we increment
     * the existing value of the litLength or matchLength by 0x10000.
     */
    ZSTD_longLengthType_e longLengthType;
    U32                   longLengthPos;  /* Index of the sequence to apply long length modification to */
} SeqStore_t;

typedef struct {
    U32 litLength;
    U32 matchLength;
} ZSTD_SequenceLength;

/**
 * Returns the ZSTD_SequenceLength for the given sequences. It handles the decoding of long sequences
 * indicated by longLengthPos and longLengthType, and adds MINMATCH back to matchLength.
 */
MEM_STATIC ZSTD_SequenceLength ZSTD_getSequenceLength(SeqStore_t const* seqStore, SeqDef const* seq)
{
    ZSTD_SequenceLength seqLen;
    seqLen.litLength = seq->litLength;
    seqLen.matchLength = seq->mlBase + MINMATCH;
    if (seqStore->longLengthPos == (U32)(seq - seqStore->sequencesStart)) {
        if (seqStore->longLengthType == ZSTD_llt_literalLength) {
            seqLen.litLength += 0x10000;
        }
        if (seqStore->longLengthType == ZSTD_llt_matchLength) {
            seqLen.matchLength += 0x10000;
        }
    }
    return seqLen;
}

const SeqStore_t* ZSTD_getSeqStore(const ZSTD_CCtx* ctx);   /* compress & dictBuilder */
int ZSTD_seqToCodes(const SeqStore_t* seqStorePtr);   /* compress, dictBuilder, decodeCorpus (shouldn't get its definition from here) */


/***********************************************
*  Entropy buffer statistics structs and funcs *
***********************************************/
/** ZSTD_hufCTablesMetadata_t :
 *  Stores Literals Block Type for a super-block in hType, and
 *  huffman tree description in hufDesBuffer.
 *  hufDesSize refers to the size of huffman tree description in bytes.
 *  This metadata is populated in ZSTD_buildBlockEntropyStats_literals() */
typedef struct {
    SymbolEncodingType_e hType;
    BYTE hufDesBuffer[ZSTD_MAX_HUF_HEADER_SIZE];
    size_t hufDesSize;
} ZSTD_hufCTablesMetadata_t;

/** ZSTD_fseCTablesMetadata_t :
 *  Stores symbol compression modes for a super-block in {ll, ol, ml}Type, and
 *  fse tables in fseTablesBuffer.
 *  fseTablesSize refers to the size of fse tables in bytes.
 *  This metadata is populated in ZSTD_buildBlockEntropyStats_sequences() */
typedef struct {
    SymbolEncodingType_e llType;
    SymbolEncodingType_e ofType;
    SymbolEncodingType_e mlType;
    BYTE fseTablesBuffer[ZSTD_MAX_FSE_HEADERS_SIZE];
    size_t fseTablesSize;
    size_t lastCountSize; /* This is to account for bug in 1.3.4. More detail in ZSTD_entropyCompressSeqStore_internal() */
} ZSTD_fseCTablesMetadata_t;

typedef struct {
    ZSTD_hufCTablesMetadata_t hufMetadata;
    ZSTD_fseCTablesMetadata_t fseMetadata;
} ZSTD_entropyCTablesMetadata_t;

/** ZSTD_buildBlockEntropyStats() :
 *  Builds entropy for the block.
 *  @return : 0 on success or error code */
size_t ZSTD_buildBlockEntropyStats(
                    const SeqStore_t* seqStorePtr,
                    const ZSTD_entropyCTables_t* prevEntropy,
                          ZSTD_entropyCTables_t* nextEntropy,
                    const ZSTD_CCtx_params* cctxParams,
                          ZSTD_entropyCTablesMetadata_t* entropyMetadata,
                          void* workspace, size_t wkspSize);

/*********************************
*  Compression internals structs *
*********************************/

typedef struct {
    U32 off;            /* Offset sumtype code for the match, using ZSTD_storeSeq() format */
    U32 len;            /* Raw length of match */
} ZSTD_match_t;

typedef struct {
    U32 offset;         /* Offset of sequence */
    U32 litLength;      /* Length of literals prior to match */
    U32 matchLength;    /* Raw length of match */
} rawSeq;

typedef struct {
  rawSeq* seq;          /* The start of the sequences */
  size_t pos;           /* The index in seq where reading stopped. pos <= size. */
  size_t posInSequence; /* The position within the sequence at seq[pos] where reading
                           stopped. posInSequence <= seq[pos].litLength + seq[pos].matchLength */
  size_t size;          /* The number of sequences. <= capacity. */
  size_t capacity;      /* The capacity starting from `seq` pointer */
} RawSeqStore_t;

UNUSED_ATTR static const RawSeqStore_t kNullRawSeqStore = {NULL, 0, 0, 0, 0};

typedef struct {
    int price;  /* price from beginning of segment to this position */
    U32 off;    /* offset of previous match */
    U32 mlen;   /* length of previous match */
    U32 litlen; /* nb of literals since previous match */
    U32 rep[ZSTD_REP_NUM];  /* offset history after previous match */
} ZSTD_optimal_t;

typedef enum { zop_dynamic=0, zop_predef } ZSTD_OptPrice_e;

#define ZSTD_OPT_SIZE (ZSTD_OPT_NUM+3)
typedef struct {
    /* All tables are allocated inside cctx->workspace by ZSTD_resetCCtx_internal() */
    unsigned* litFreq;           /* table of literals statistics, of size 256 */
    unsigned* litLengthFreq;     /* table of litLength statistics, of size (MaxLL+1) */
    unsigned* matchLengthFreq;   /* table of matchLength statistics, of size (MaxML+1) */
    unsigned* offCodeFreq;       /* table of offCode statistics, of size (MaxOff+1) */
    ZSTD_match_t* matchTable;    /* list of found matches, of size ZSTD_OPT_SIZE */
    ZSTD_optimal_t* priceTable;  /* All positions tracked by optimal parser, of size ZSTD_OPT_SIZE */

    U32  litSum;                 /* nb of literals */
    U32  litLengthSum;           /* nb of litLength codes */
    U32  matchLengthSum;         /* nb of matchLength codes */
    U32  offCodeSum;             /* nb of offset codes */
    U32  litSumBasePrice;        /* to compare to log2(litfreq) */
    U32  litLengthSumBasePrice;  /* to compare to log2(llfreq)  */
    U32  matchLengthSumBasePrice;/* to compare to log2(mlfreq)  */
    U32  offCodeSumBasePrice;    /* to compare to log2(offreq)  */
    ZSTD_OptPrice_e priceType;   /* prices can be determined dynamically, or follow a pre-defined cost structure */
    const ZSTD_entropyCTables_t* symbolCosts;  /* pre-calculated dictionary statistics */
    ZSTD_ParamSwitch_e literalCompressionMode;
} optState_t;

typedef struct {
  ZSTD_entropyCTables_t entropy;
  U32 rep[ZSTD_REP_NUM];
} ZSTD_compressedBlockState_t;

typedef struct {
    BYTE const* nextSrc;       /* next block here to continue on current prefix */
    BYTE const* base;          /* All regular indexes relative to this position */
    BYTE const* dictBase;      /* extDict indexes relative to this position */
    U32 dictLimit;             /* below that point, need extDict */
    U32 lowLimit;              /* below that point, no more valid data */
    U32 nbOverflowCorrections; /* Number of times overflow correction has run since
                                * ZSTD_window_init(). Useful for debugging coredumps
                                * and for ZSTD_WINDOW_OVERFLOW_CORRECT_FREQUENTLY.
                                */
} ZSTD_window_t;

#define ZSTD_WINDOW_START_INDEX 2

typedef struct ZSTD_MatchState_t ZSTD_MatchState_t;

#define ZSTD_ROW_HASH_CACHE_SIZE 8       /* Size of prefetching hash cache for row-based matchfinder */

struct ZSTD_MatchState_t {
    ZSTD_window_t window;   /* State for window round buffer management */
    U32 loadedDictEnd;      /* index of end of dictionary, within context's referential.
                             * When loadedDictEnd != 0, a dictionary is in use, and still valid.
                             * This relies on a mechanism to set loadedDictEnd=0 when dictionary is no longer within distance.
                             * Such mechanism is provided within ZSTD_window_enforceMaxDist() and ZSTD_checkDictValidity().
                             * When dict referential is copied into active context (i.e. not attached),
                             * loadedDictEnd == dictSize, since referential starts from zero.
                             */
    U32 nextToUpdate;       /* index from which to continue table update */
    U32 hashLog3;           /* dispatch table for matches of len==3 : larger == faster, more memory */

    U32 rowHashLog;                          /* For row-based matchfinder: Hashlog based on nb of rows in the hashTable.*/
    BYTE* tagTable;                          /* For row-based matchFinder: A row-based table containing the hashes and head index. */
    U32 hashCache[ZSTD_ROW_HASH_CACHE_SIZE]; /* For row-based matchFinder: a cache of hashes to improve speed */
    U64 hashSalt;                            /* For row-based matchFinder: salts the hash for reuse of tag table */
    U32 hashSaltEntropy;                     /* For row-based matchFinder: collects entropy for salt generation */

    U32* hashTable;
    U32* hashTable3;
    U32* chainTable;

    int forceNonContiguous; /* Non-zero if we should force non-contiguous load for the next window update. */

    int dedicatedDictSearch;  /* Indicates whether this matchState is using the
                               * dedicated dictionary search structure.
                               */
    optState_t opt;         /* optimal parser state */
    const ZSTD_MatchState_t* dictMatchState;
    ZSTD_compressionParameters cParams;
    const RawSeqStore_t* ldmSeqStore;

    /* Controls prefetching in some dictMatchState matchfinders.
     * This behavior is controlled from the cctx ms.
     * This parameter has no effect in the cdict ms. */
    int prefetchCDictTables;

    /* When == 0, lazy match finders insert every position.
     * When != 0, lazy match finders only insert positions they search.
     * This allows them to skip much faster over incompressible data,
     * at a small cost to compression ratio.
     */
    int lazySkipping;
};

typedef struct {
    ZSTD_compressedBlockState_t* prevCBlock;
    ZSTD_compressedBlockState_t* nextCBlock;
    ZSTD_MatchState_t matchState;
} ZSTD_blockState_t;

typedef struct {
    U32 offset;
    U32 checksum;
} ldmEntry_t;

typedef struct {
    BYTE const* split;
    U32 hash;
    U32 checksum;
    ldmEntry_t* bucket;
} ldmMatchCandidate_t;

#define LDM_BATCH_SIZE 64

typedef struct {
    ZSTD_window_t window;   /* State for the window round buffer management */
    ldmEntry_t* hashTable;
    U32 loadedDictEnd;
    BYTE* bucketOffsets;    /* Next position in bucket to insert entry */
    size_t splitIndices[LDM_BATCH_SIZE];
    ldmMatchCandidate_t matchCandidates[LDM_BATCH_SIZE];
} ldmState_t;

typedef struct {
    ZSTD_ParamSwitch_e enableLdm; /* ZSTD_ps_enable to enable LDM. ZSTD_ps_auto by default */
    U32 hashLog;            /* Log size of hashTable */
    U32 bucketSizeLog;      /* Log bucket size for collision resolution, at most 8 */
    U32 minMatchLength;     /* Minimum match length */
    U32 hashRateLog;       /* Log number of entries to skip */
    U32 windowLog;          /* Window log for the LDM */
} ldmParams_t;

typedef struct {
    int collectSequences;
    ZSTD_Sequence* seqStart;
    size_t seqIndex;
    size_t maxSequences;
} SeqCollector;

struct ZSTD_CCtx_params_s {
    ZSTD_format_e format;
    ZSTD_compressionParameters cParams;
    ZSTD_frameParameters fParams;

    int compressionLevel;
    int forceWindow;           /* force back-references to respect limit of
                                * 1<<wLog, even for dictionary */
    size_t targetCBlockSize;   /* Tries to fit compressed block size to be around targetCBlockSize.
                                * No target when targetCBlockSize == 0.
                                * There is no guarantee on compressed block size */
    int srcSizeHint;           /* User's best guess of source size.
                                * Hint is not valid when srcSizeHint == 0.
                                * There is no guarantee that hint is close to actual source size */

    ZSTD_dictAttachPref_e attachDictPref;
    ZSTD_ParamSwitch_e literalCompressionMode;

    /* Multithreading: used to pass parameters to mtctx */
    int nbWorkers;
    size_t jobSize;
    int overlapLog;
    int rsyncable;

    /* Long distance matching parameters */
    ldmParams_t ldmParams;

    /* Dedicated dict search algorithm trigger */
    int enableDedicatedDictSearch;

    /* Input/output buffer modes */
    ZSTD_bufferMode_e inBufferMode;
    ZSTD_bufferMode_e outBufferMode;

    /* Sequence compression API */
    ZSTD_SequenceFormat_e blockDelimiters;
    int validateSequences;

    /* Block splitting
     * @postBlockSplitter executes split analysis after sequences are produced,
     * it's more accurate but consumes more resources.
     * @preBlockSplitter_level splits before knowing sequences,
     * it's more approximative but also cheaper.
     * Valid @preBlockSplitter_level values range from 0 to 6 (included).
     * 0 means auto, 1 means do not split,
     * then levels are sorted in increasing cpu budget, from 2 (fastest) to 6 (slowest).
     * Highest @preBlockSplitter_level combines well with @postBlockSplitter.
     */
    ZSTD_ParamSwitch_e postBlockSplitter;
    int preBlockSplitter_level;

    /* Adjust the max block size*/
    size_t maxBlockSize;

    /* Param for deciding whether to use row-based matchfinder */
    ZSTD_ParamSwitch_e useRowMatchFinder;

    /* Always load a dictionary in ext-dict mode (not prefix mode)? */
    int deterministicRefPrefix;

    /* Internal use, for createCCtxParams() and freeCCtxParams() only */
    ZSTD_customMem customMem;

    /* Controls prefetching in some dictMatchState matchfinders */
    ZSTD_ParamSwitch_e prefetchCDictTables;

    /* Controls whether zstd will fall back to an internal matchfinder
     * if the external matchfinder returns an error code. */
    int enableMatchFinderFallback;

    /* Parameters for the external sequence producer API.
     * Users set these parameters through ZSTD_registerSequenceProducer().
     * It is not possible to set these parameters individually through the public API. */
    void* extSeqProdState;
    ZSTD_sequenceProducer_F extSeqProdFunc;

    /* Controls repcode search in external sequence parsing */
    ZSTD_ParamSwitch_e searchForExternalRepcodes;
};  /* typedef'd to ZSTD_CCtx_params within "zstd.h" */

#define COMPRESS_SEQUENCES_WORKSPACE_SIZE (sizeof(unsigned) * (MaxSeq + 2))
#define ENTROPY_WORKSPACE_SIZE (HUF_WORKSPACE_SIZE + COMPRESS_SEQUENCES_WORKSPACE_SIZE)
#define TMP_WORKSPACE_SIZE (MAX(ENTROPY_WORKSPACE_SIZE, ZSTD_SLIPBLOCK_WORKSPACESIZE))

/**
 * Indicates whether this compression proceeds directly from user-provided
 * source buffer to user-provided destination buffer (ZSTDb_not_buffered), or
 * whether the context needs to buffer the input/output (ZSTDb_buffered).
 */
typedef enum {
    ZSTDb_not_buffered,
    ZSTDb_buffered
} ZSTD_buffered_policy_e;

/**
 * Struct that contains all elements of block splitter that should be allocated
 * in a wksp.
 */
#define ZSTD_MAX_NB_BLOCK_SPLITS 196
typedef struct {
    SeqStore_t fullSeqStoreChunk;
    SeqStore_t firstHalfSeqStore;
    SeqStore_t secondHalfSeqStore;
    SeqStore_t currSeqStore;
    SeqStore_t nextSeqStore;

    U32 partitions[ZSTD_MAX_NB_BLOCK_SPLITS];
    ZSTD_entropyCTablesMetadata_t entropyMetadata;
} ZSTD_blockSplitCtx;

struct ZSTD_CCtx_s {
    ZSTD_compressionStage_e stage;
    int cParamsChanged;                  /* == 1 if cParams(except wlog) or compression level are changed in requestedParams. Triggers transmission of new params to ZSTDMT (if available) then reset to 0. */
    int bmi2;                            /* == 1 if the CPU supports BMI2 and 0 otherwise. CPU support is determined dynamically once per context lifetime. */
    ZSTD_CCtx_params requestedParams;
    ZSTD_CCtx_params appliedParams;
    ZSTD_CCtx_params simpleApiParams;    /* Param storage used by the simple API - not sticky. Must only be used in top-level simple API functions for storage. */
    U32   dictID;
    size_t dictContentSize;

    ZSTD_cwksp workspace; /* manages buffer for dynamic allocations */
    size_t blockSizeMax;
    unsigned long long pledgedSrcSizePlusOne;  /* this way, 0 (default) == unknown */
    unsigned long long consumedSrcSize;
    unsigned long long producedCSize;
    XXH64_state_t xxhState;
    ZSTD_customMem customMem;
    ZSTD_threadPool* pool;
    size_t staticSize;
    SeqCollector seqCollector;
    int isFirstBlock;
    int initialized;

    SeqStore_t seqStore;      /* sequences storage ptrs */
    ldmState_t ldmState;      /* long distance matching state */
    rawSeq* ldmSequences;     /* Storage for the ldm output sequences */
    size_t maxNbLdmSequences;
    RawSeqStore_t externSeqStore; /* Mutable reference to external sequences */
    ZSTD_blockState_t blockState;
    void* tmpWorkspace;  /* used as substitute of stack space - must be aligned for S64 type */
    size_t tmpWkspSize;

    /* Whether we are streaming or not */
    ZSTD_buffered_policy_e bufferedPolicy;

    /* streaming */
    char*  inBuff;
    size_t inBuffSize;
    size_t inToCompress;
    size_t inBuffPos;
    size_t inBuffTarget;
    char*  outBuff;
    size_t outBuffSize;
    size_t outBuffContentSize;
    size_t outBuffFlushedSize;
    ZSTD_cStreamStage streamStage;
    U32    frameEnded;

    /* Stable in/out buffer verification */
    ZSTD_inBuffer expectedInBuffer;
    size_t stableIn_notConsumed; /* nb bytes within stable input buffer that are said to be consumed but are not */
    size_t expectedOutBufferSize;

    /* Dictionary */
    ZSTD_localDict localDict;
    const ZSTD_CDict* cdict;
    ZSTD_prefixDict prefixDict;   /* single-usage dictionary */

    /* Multi-threading */
#ifdef ZSTD_MULTITHREAD
    ZSTDMT_CCtx* mtctx;
#endif

    /* Tracing */
#if ZSTD_TRACE
    ZSTD_TraceCtx traceCtx;
#endif

    /* Workspace for block splitter */
    ZSTD_blockSplitCtx blockSplitCtx;

    /* Buffer for output from external sequence producer */
    ZSTD_Sequence* extSeqBuf;
    size_t extSeqBufCapacity;
};

typedef enum { ZSTD_dtlm_fast, ZSTD_dtlm_full } ZSTD_dictTableLoadMethod_e;
typedef enum { ZSTD_tfp_forCCtx, ZSTD_tfp_forCDict } ZSTD_tableFillPurpose_e;

typedef enum {
    ZSTD_noDict = 0,
    ZSTD_extDict = 1,
    ZSTD_dictMatchState = 2,
    ZSTD_dedicatedDictSearch = 3
} ZSTD_dictMode_e;

typedef enum {
    ZSTD_cpm_noAttachDict = 0,  /* Compression with ZSTD_noDict or ZSTD_extDict.
                                 * In this mode we use both the srcSize and the dictSize
                                 * when selecting and adjusting parameters.
                                 */
    ZSTD_cpm_attachDict = 1,    /* Compression with ZSTD_dictMatchState or ZSTD_dedicatedDictSearch.
                                 * In this mode we only take the srcSize into account when selecting
                                 * and adjusting parameters.
                                 */
    ZSTD_cpm_createCDict = 2,   /* Creating a CDict.
                                 * In this mode we take both the source size and the dictionary size
                                 * into account when selecting and adjusting the parameters.
                                 */
    ZSTD_cpm_unknown = 3        /* ZSTD_getCParams, ZSTD_getParams, ZSTD_adjustParams.
                                 * We don't know what these parameters are for. We default to the legacy
                                 * behavior of taking both the source size and the dict size into account
                                 * when selecting and adjusting parameters.
                                 */
} ZSTD_CParamMode_e;

typedef size_t (*ZSTD_BlockCompressor_f) (
        ZSTD_MatchState_t* bs, SeqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize);
ZSTD_BlockCompressor_f ZSTD_selectBlockCompressor(ZSTD_strategy strat, ZSTD_ParamSwitch_e rowMatchfinderMode, ZSTD_dictMode_e dictMode);


MEM_STATIC U32 ZSTD_LLcode(U32 litLength)
{
    static const BYTE LL_Code[64] = {  0,  1,  2,  3,  4,  5,  6,  7,
                                       8,  9, 10, 11, 12, 13, 14, 15,
                                      16, 16, 17, 17, 18, 18, 19, 19,
                                      20, 20, 20, 20, 21, 21, 21, 21,
                                      22, 22, 22, 22, 22, 22, 22, 22,
                                      23, 23, 23, 23, 23, 23, 23, 23,
                                      24, 24, 24, 24, 24, 24, 24, 24,
                                      24, 24, 24, 24, 24, 24, 24, 24 };
    static const U32 LL_deltaCode = 19;
    return (litLength > 63) ? ZSTD_highbit32(litLength) + LL_deltaCode : LL_Code[litLength];
}

/* ZSTD_MLcode() :
 * note : mlBase = matchLength - MINMATCH;
 *        because it's the format it's stored in seqStore->sequences */
MEM_STATIC U32 ZSTD_MLcode(U32 mlBase)
{
    static const BYTE ML_Code[128] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
                                      16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                                      32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 36, 36, 37, 37, 37, 37,
                                      38, 38, 38, 38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 39, 39, 39,
                                      40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
                                      41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
                                      42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
                                      42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42 };
    static const U32 ML_deltaCode = 36;
    return (mlBase > 127) ? ZSTD_highbit32(mlBase) + ML_deltaCode : ML_Code[mlBase];
}

/* ZSTD_cParam_withinBounds:
 * @return 1 if value is within cParam bounds,
 * 0 otherwise */
MEM_STATIC int ZSTD_cParam_withinBounds(ZSTD_cParameter cParam, int value)
{
    ZSTD_bounds const bounds = ZSTD_cParam_getBounds(cParam);
    if (ZSTD_isError(bounds.error)) return 0;
    if (value < bounds.lowerBound) return 0;
    if (value > bounds.upperBound) return 0;
    return 1;
}

/* ZSTD_selectAddr:
 * @return index >= lowLimit ? candidate : backup,
 * tries to force branchless codegen. */
MEM_STATIC const BYTE*
ZSTD_selectAddr(U32 index, U32 lowLimit, const BYTE* candidate, const BYTE* backup)
{
#if defined(__GNUC__) && defined(__x86_64__)
    __asm__ (
        "cmp %1, %2\n"
        "cmova %3, %0\n"
        : "+r"(candidate)
        : "r"(index), "r"(lowLimit), "r"(backup)
        );
    return candidate;
#else
    return index >= lowLimit ? candidate : backup;
#endif
}

/* ZSTD_noCompressBlock() :
 * Writes uncompressed block to dst buffer from given src.
 * Returns the size of the block */
MEM_STATIC size_t
ZSTD_noCompressBlock(void* dst, size_t dstCapacity, const void* src, size_t srcSize, U32 lastBlock)
{
    U32 const cBlockHeader24 = lastBlock + (((U32)bt_raw)<<1) + (U32)(srcSize << 3);
    DEBUGLOG(5, "ZSTD_noCompressBlock (srcSize=%zu, dstCapacity=%zu)", srcSize, dstCapacity);
    RETURN_ERROR_IF(srcSize + ZSTD_blockHeaderSize > dstCapacity,
                    dstSize_tooSmall, "dst buf too small for uncompressed block");
    MEM_writeLE24(dst, cBlockHeader24);
    ZSTD_memcpy((BYTE*)dst + ZSTD_blockHeaderSize, src, srcSize);
    return ZSTD_blockHeaderSize + srcSize;
}

MEM_STATIC size_t
ZSTD_rleCompressBlock(void* dst, size_t dstCapacity, BYTE src, size_t srcSize, U32 lastBlock)
{
    BYTE* const op = (BYTE*)dst;
    U32 const cBlockHeader = lastBlock + (((U32)bt_rle)<<1) + (U32)(srcSize << 3);
    RETURN_ERROR_IF(dstCapacity < 4, dstSize_tooSmall, "");
    MEM_writeLE24(op, cBlockHeader);
    op[3] = src;
    return 4;
}


/* ZSTD_minGain() :
 * minimum compression required
 * to generate a compress block or a compressed literals section.
 * note : use same formula for both situations */
MEM_STATIC size_t ZSTD_minGain(size_t srcSize, ZSTD_strategy strat)
{
    U32 const minlog = (strat>=ZSTD_btultra) ? (U32)(strat) - 1 : 6;
    ZSTD_STATIC_ASSERT(ZSTD_btultra == 8);
    assert(ZSTD_cParam_withinBounds(ZSTD_c_strategy, (int)strat));
    return (srcSize >> minlog) + 2;
}

MEM_STATIC int ZSTD_literalsCompressionIsDisabled(const ZSTD_CCtx_params* cctxParams)
{
    switch (cctxParams->literalCompressionMode) {
    case ZSTD_ps_enable:
        return 0;
    case ZSTD_ps_disable:
        return 1;
    default:
        assert(0 /* impossible: pre-validated */);
        ZSTD_FALLTHROUGH;
    case ZSTD_ps_auto:
        return (cctxParams->cParams.strategy == ZSTD_fast) && (cctxParams->cParams.targetLength > 0);
    }
}

/*! ZSTD_safecopyLiterals() :
 *  memcpy() function that won't read beyond more than WILDCOPY_OVERLENGTH bytes past ilimit_w.
 *  Only called when the sequence ends past ilimit_w, so it only needs to be optimized for single
 *  large copies.
 */
static void
ZSTD_safecopyLiterals(BYTE* op, BYTE const* ip, BYTE const* const iend, BYTE const* ilimit_w)
{
    assert(iend > ilimit_w);
    if (ip <= ilimit_w) {
        ZSTD_wildcopy(op, ip, (size_t)(ilimit_w - ip), ZSTD_no_overlap);
        op += ilimit_w - ip;
        ip = ilimit_w;
    }
    while (ip < iend) *op++ = *ip++;
}


#define REPCODE1_TO_OFFBASE REPCODE_TO_OFFBASE(1)
#define REPCODE2_TO_OFFBASE REPCODE_TO_OFFBASE(2)
#define REPCODE3_TO_OFFBASE REPCODE_TO_OFFBASE(3)
#define REPCODE_TO_OFFBASE(r) (assert((r)>=1), assert((r)<=ZSTD_REP_NUM), (r)) /* accepts IDs 1,2,3 */
#define OFFSET_TO_OFFBASE(o)  (assert((o)>0), o + ZSTD_REP_NUM)
#define OFFBASE_IS_OFFSET(o)  ((o) > ZSTD_REP_NUM)
#define OFFBASE_IS_REPCODE(o) ( 1 <= (o) && (o) <= ZSTD_REP_NUM)
#define OFFBASE_TO_OFFSET(o)  (assert(OFFBASE_IS_OFFSET(o)), (o) - ZSTD_REP_NUM)
#define OFFBASE_TO_REPCODE(o) (assert(OFFBASE_IS_REPCODE(o)), (o))  /* returns ID 1,2,3 */

/*! ZSTD_storeSeqOnly() :
 *  Store a sequence (litlen, litPtr, offBase and matchLength) into SeqStore_t.
 *  Literals themselves are not copied, but @litPtr is updated.
 *  @offBase : Users should employ macros REPCODE_TO_OFFBASE() and OFFSET_TO_OFFBASE().
 *  @matchLength : must be >= MINMATCH
*/
HINT_INLINE UNUSED_ATTR void
ZSTD_storeSeqOnly(SeqStore_t* seqStorePtr,
              size_t litLength,
              U32 offBase,
              size_t matchLength)
{
    assert((size_t)(seqStorePtr->sequences - seqStorePtr->sequencesStart) < seqStorePtr->maxNbSeq);

    /* literal Length */
    assert(litLength <= ZSTD_BLOCKSIZE_MAX);
    if (UNLIKELY(litLength>0xFFFF)) {
        assert(seqStorePtr->longLengthType == ZSTD_llt_none); /* there can only be a single long length */
        seqStorePtr->longLengthType = ZSTD_llt_literalLength;
        seqStorePtr->longLengthPos = (U32)(seqStorePtr->sequences - seqStorePtr->sequencesStart);
    }
    seqStorePtr->sequences[0].litLength = (U16)litLength;

    /* match offset */
    seqStorePtr->sequences[0].offBase = offBase;

    /* match Length */
    assert(matchLength <= ZSTD_BLOCKSIZE_MAX);
    assert(matchLength >= MINMATCH);
    {   size_t const mlBase = matchLength - MINMATCH;
        if (UNLIKELY(mlBase>0xFFFF)) {
            assert(seqStorePtr->longLengthType == ZSTD_llt_none); /* there can only be a single long length */
            seqStorePtr->longLengthType = ZSTD_llt_matchLength;
            seqStorePtr->longLengthPos = (U32)(seqStorePtr->sequences - seqStorePtr->sequencesStart);
        }
        seqStorePtr->sequences[0].mlBase = (U16)mlBase;
    }

    seqStorePtr->sequences++;
}

/*! ZSTD_storeSeq() :
 *  Store a sequence (litlen, litPtr, offBase and matchLength) into SeqStore_t.
 *  @offBase : Users should employ macros REPCODE_TO_OFFBASE() and OFFSET_TO_OFFBASE().
 *  @matchLength : must be >= MINMATCH
 *  Allowed to over-read literals up to litLimit.
*/
HINT_INLINE UNUSED_ATTR void
ZSTD_storeSeq(SeqStore_t* seqStorePtr,
              size_t litLength, const BYTE* literals, const BYTE* litLimit,
              U32 offBase,
              size_t matchLength)
{
    BYTE const* const litLimit_w = litLimit - WILDCOPY_OVERLENGTH;
    BYTE const* const litEnd = literals + litLength;
#if defined(DEBUGLEVEL) && (DEBUGLEVEL >= 6)
    static const BYTE* g_start = NULL;
    if (g_start==NULL) g_start = (const BYTE*)literals;  /* note : index only works for compression within a single segment */
    {   U32 const pos = (U32)((const BYTE*)literals - g_start);
        DEBUGLOG(6, "Cpos%7u :%3u literals, match%4u bytes at offBase%7u",
               pos, (U32)litLength, (U32)matchLength, (U32)offBase);
    }
#endif
    assert((size_t)(seqStorePtr->sequences - seqStorePtr->sequencesStart) < seqStorePtr->maxNbSeq);
    /* copy Literals */
    assert(seqStorePtr->maxNbLit <= 128 KB);
    assert(seqStorePtr->lit + litLength <= seqStorePtr->litStart + seqStorePtr->maxNbLit);
    assert(literals + litLength <= litLimit);
    if (litEnd <= litLimit_w) {
        /* Common case we can use wildcopy.
         * First copy 16 bytes, because literals are likely short.
         */
        ZSTD_STATIC_ASSERT(WILDCOPY_OVERLENGTH >= 16);
        ZSTD_copy16(seqStorePtr->lit, literals);
        if (litLength > 16) {
            ZSTD_wildcopy(seqStorePtr->lit+16, literals+16, litLength-16, ZSTD_no_overlap);
        }
    } else {
        ZSTD_safecopyLiterals(seqStorePtr->lit, literals, litEnd, litLimit_w);
    }
    seqStorePtr->lit += litLength;

    ZSTD_storeSeqOnly(seqStorePtr, litLength, offBase, matchLength);
}

/* ZSTD_updateRep() :
 * updates in-place @rep (array of repeat offsets)
 * @offBase : sum-type, using numeric representation of ZSTD_storeSeq()
 */
MEM_STATIC void
ZSTD_updateRep(U32 rep[ZSTD_REP_NUM], U32 const offBase, U32 const ll0)
{
    if (OFFBASE_IS_OFFSET(offBase)) {  /* full offset */
        rep[2] = rep[1];
        rep[1] = rep[0];
        rep[0] = OFFBASE_TO_OFFSET(offBase);
    } else {   /* repcode */
        U32 const repCode = OFFBASE_TO_REPCODE(offBase) - 1 + ll0;
        if (repCode > 0) {  /* note : if repCode==0, no change */
            U32 const currentOffset = (repCode==ZSTD_REP_NUM) ? (rep[0] - 1) : rep[repCode];
            rep[2] = (repCode >= 2) ? rep[1] : rep[2];
            rep[1] = rep[0];
            rep[0] = currentOffset;
        } else {   /* repCode == 0 */
            /* nothing to do */
        }
    }
}

typedef struct repcodes_s {
    U32 rep[3];
} Repcodes_t;

MEM_STATIC Repcodes_t
ZSTD_newRep(U32 const rep[ZSTD_REP_NUM], U32 const offBase, U32 const ll0)
{
    Repcodes_t newReps;
    ZSTD_memcpy(&newReps, rep, sizeof(newReps));
    ZSTD_updateRep(newReps.rep, offBase, ll0);
    return newReps;
}


/*-*************************************
*  Match length counter
***************************************/
MEM_STATIC size_t ZSTD_count(const BYTE* pIn, const BYTE* pMatch, const BYTE* const pInLimit)
{
    const BYTE* const pStart = pIn;
    const BYTE* const pInLoopLimit = pInLimit - (sizeof(size_t)-1);

    if (pIn < pInLoopLimit) {
        { size_t const diff = MEM_readST(pMatch) ^ MEM_readST(pIn);
          if (diff) return ZSTD_NbCommonBytes(diff); }
        pIn+=sizeof(size_t); pMatch+=sizeof(size_t);
        while (pIn < pInLoopLimit) {
            size_t const diff = MEM_readST(pMatch) ^ MEM_readST(pIn);
            if (!diff) { pIn+=sizeof(size_t); pMatch+=sizeof(size_t); continue; }
            pIn += ZSTD_NbCommonBytes(diff);
            return (size_t)(pIn - pStart);
    }   }
    if (MEM_64bits() && (pIn<(pInLimit-3)) && (MEM_read32(pMatch) == MEM_read32(pIn))) { pIn+=4; pMatch+=4; }
    if ((pIn<(pInLimit-1)) && (MEM_read16(pMatch) == MEM_read16(pIn))) { pIn+=2; pMatch+=2; }
    if ((pIn<pInLimit) && (*pMatch == *pIn)) pIn++;
    return (size_t)(pIn - pStart);
}

/** ZSTD_count_2segments() :
 *  can count match length with `ip` & `match` in 2 different segments.
 *  convention : on reaching mEnd, match count continue starting from iStart
 */
MEM_STATIC size_t
ZSTD_count_2segments(const BYTE* ip, const BYTE* match,
                     const BYTE* iEnd, const BYTE* mEnd, const BYTE* iStart)
{
    const BYTE* const vEnd = MIN( ip + (mEnd - match), iEnd);
    size_t const matchLength = ZSTD_count(ip, match, vEnd);
    if (match + matchLength != mEnd) return matchLength;
    DEBUGLOG(7, "ZSTD_count_2segments: found a 2-parts match (current length==%zu)", matchLength);
    DEBUGLOG(7, "distance from match beginning to end dictionary = %i", (int)(mEnd - match));
    DEBUGLOG(7, "distance from current pos to end buffer = %i", (int)(iEnd - ip));
    DEBUGLOG(7, "next byte : ip==%02X, istart==%02X", ip[matchLength], *iStart);
    DEBUGLOG(7, "final match length = %zu", matchLength + ZSTD_count(ip+matchLength, iStart, iEnd));
    return matchLength + ZSTD_count(ip+matchLength, iStart, iEnd);
}

MEM_STATIC U32 ZSTD_reduceHash32(U32 hash, U32 bits) {
    assert(bits <= 32);
#if defined(ZSTD_HASH_INTERNAL_CRC32_U64)
    return hash & ((1ULL << bits) - 1);
#else
    return hash >> (32 - bits);
#endif
}

MEM_STATIC size_t ZSTD_reduceHash64(U64 hash, U32 bits) {
    assert(bits <= 32);
#if defined(ZSTD_HASH_INTERNAL_CRC32_U64)
    return (size_t)(hash & ((1ULL << bits) - 1));
#else
    return (size_t)(hash >> (64 - bits));
#endif
}

/*-*************************************
 *  Hashes
 ***************************************/
static const U32 prime3bytes = 506832829U;
static const U32 prime4bytes = 2654435761U;
static const U64 prime5bytes = 889523592379ULL;
static const U64 prime6bytes = 227718039650203ULL;
static const U64 prime7bytes = 58295818150454627ULL;
static const U64 prime8bytes = 0xCF1BBCDCB7A56463ULL;

#if defined(ZSTD_HASH_USE_CRC32C)

static U32    ZSTD_hash3(U32 u, U32 h, U32 s) { assert(h <= 32); return ZSTD_reduceHash32((U32)ZSTD_HASH_INTERNAL_CRC32_U64((U32)s, u << 8), h); }
static U32    ZSTD_hash4(U32 u, U32 h, U32 s) { assert(h <= 32); return ZSTD_reduceHash32((U32)ZSTD_HASH_INTERNAL_CRC32_U64((U32)s, u), h); }
static size_t ZSTD_hash5(U64 u, U32 h, U64 s) { assert(h <= 64); return ZSTD_reduceHash64((U64)(ZSTD_HASH_INTERNAL_CRC32_U64((U32)s, u << 24)), h); }
static size_t ZSTD_hash6(U64 u, U32 h, U64 s) { assert(h <= 64); return ZSTD_reduceHash64((U64)(ZSTD_HASH_INTERNAL_CRC32_U64((U32)s, u << 16)), h); }
static size_t ZSTD_hash7(U64 u, U32 h, U64 s) { assert(h <= 64); return ZSTD_reduceHash64((U64)(ZSTD_HASH_INTERNAL_CRC32_U64((U32)s, u << 8)), h); }
static size_t ZSTD_hash8(U64 u, U32 h, U64 s) { assert(h <= 64); return ZSTD_reduceHash64((U64)(ZSTD_HASH_INTERNAL_CRC32_U64((U32)s, u)), h); }

#else

static U32    ZSTD_hash3(U32 u, U32 h, U32 s) { assert(h <= 32); return ZSTD_reduceHash32((((u << (32-24)) * prime3bytes) ^ s) , h) ; }
static U32    ZSTD_hash4(U32 u, U32 h, U32 s) { assert(h <= 32); return ZSTD_reduceHash32(((u * prime4bytes) ^ s), h) ; }
static size_t ZSTD_hash5(U64 u, U32 h, U64 s) { assert(h <= 64); return ZSTD_reduceHash64((U64)(((u  << (64-40)) * prime5bytes) ^ s), h) ; }
static size_t ZSTD_hash6(U64 u, U32 h, U64 s) { assert(h <= 64); return ZSTD_reduceHash64((U64)(((u  << (64-48)) * prime6bytes) ^ s), h) ; }
static size_t ZSTD_hash7(U64 u, U32 h, U64 s) { assert(h <= 64); return ZSTD_reduceHash64((U64)(((u  << (64-56)) * prime7bytes) ^ s), h); }
static size_t ZSTD_hash8(U64 u, U32 h, U64 s) { assert(h <= 64); return ZSTD_reduceHash64((U64)(((u) * prime8bytes) ^ s), h) ; }

#endif

MEM_STATIC size_t ZSTD_hash3Ptr(const void* ptr, U32 h) { return ZSTD_hash3(MEM_readLE32(ptr), h, 0); } /* only in zstd_opt.h */
static size_t ZSTD_hash4Ptr(const void* ptr, U32 h) { return ZSTD_hash4(MEM_readLE32(ptr), h, 0); }
static size_t ZSTD_hash5Ptr(const void* p, U32 h) { return ZSTD_hash5(MEM_readLE64(p), h, 0); }
static size_t ZSTD_hash6Ptr(const void* p, U32 h) { return ZSTD_hash6(MEM_readLE64(p), h, 0); }
static size_t ZSTD_hash7Ptr(const void* p, U32 h) { return ZSTD_hash7(MEM_readLE64(p), h, 0); }
static size_t ZSTD_hash8Ptr(const void* p, U32 h) { return ZSTD_hash8(MEM_readLE64(p), h, 0); }
MEM_STATIC size_t ZSTD_hash3PtrS(const void* ptr, U32 h, U32 s) { return ZSTD_hash3(MEM_readLE32(ptr), h, s); }
static size_t ZSTD_hash4PtrS(const void* ptr, U32 h, U32 s) { return ZSTD_hash4(MEM_readLE32(ptr), h, s); }
static size_t ZSTD_hash5PtrS(const void* p, U32 h, U64 s) { return ZSTD_hash5(MEM_readLE64(p), h, s); }
static size_t ZSTD_hash6PtrS(const void* p, U32 h, U64 s) { return ZSTD_hash6(MEM_readLE64(p), h, s); }
static size_t ZSTD_hash7PtrS(const void* p, U32 h, U64 s) { return ZSTD_hash7(MEM_readLE64(p), h, s); }
static size_t ZSTD_hash8PtrS(const void* p, U32 h, U64 s) { return ZSTD_hash8(MEM_readLE64(p), h, s); }


MEM_STATIC FORCE_INLINE_ATTR
size_t ZSTD_hashPtr(const void* p, U32 hBits, U32 mls)
{
    /* Although some of these hashes do support hBits up to 64, some do not.
     * To be on the safe side, always avoid hBits > 32. */
    assert(hBits <= 32);

    switch(mls)
    {
    default:
    case 4: return ZSTD_hash4Ptr(p, hBits);
    case 5: return ZSTD_hash5Ptr(p, hBits);
    case 6: return ZSTD_hash6Ptr(p, hBits);
    case 7: return ZSTD_hash7Ptr(p, hBits);
    case 8: return ZSTD_hash8Ptr(p, hBits);
    }
}

MEM_STATIC FORCE_INLINE_ATTR
size_t ZSTD_hashPtrSalted(const void* p, U32 hBits, U32 mls, const U64 hashSalt) {
    /* Although some of these hashes do support hBits up to 64, some do not.
     * To be on the safe side, always avoid hBits > 32. */
    assert(hBits <= 32);

    switch(mls)
    {
        default:
        case 4: return ZSTD_hash4PtrS(p, hBits, (U32)hashSalt);
        case 5: return ZSTD_hash5PtrS(p, hBits, hashSalt);
        case 6: return ZSTD_hash6PtrS(p, hBits, hashSalt);
        case 7: return ZSTD_hash7PtrS(p, hBits, hashSalt);
        case 8: return ZSTD_hash8PtrS(p, hBits, hashSalt);
    }
}

#if defined(ZSTD_HASH_INTERNAL_CRC32_U64)

MEM_STATIC FORCE_INLINE_ATTR size_t ZSTD_extractHash(size_t hashAndTag, U32 tagBits, U32 hashBits) {
    (void)tagBits;
    return hashAndTag & (((U64)1 << hashBits) - 1);
}

MEM_STATIC FORCE_INLINE_ATTR size_t ZSTD_extractIndex(size_t indexAndTag, U32 tagBits) {
    return indexAndTag & (((U32)1 << (32 - tagBits)) - 1);
}

MEM_STATIC FORCE_INLINE_ATTR U32 ZSTD_extractTagFromHash(size_t hashAndTag, U32 tagBits, U32 hashBits) {
    (void)tagBits;
    return (U32)(hashAndTag >> hashBits);
}

MEM_STATIC FORCE_INLINE_ATTR U32 ZSTD_extractTagFromIndex(size_t indexAndTag, U32 tagBits) {
    return (U32)(indexAndTag >> (32 - tagBits));
}

MEM_STATIC FORCE_INLINE_ATTR U32 ZSTD_combineIndexAndTag(U32 index, U32 tag, U32 tagBits) {
    assert(tag >> tagBits == 0);
    assert(index >> (32 - tagBits) == 0);
    return (tag << (32 - tagBits)) | index;
}

#else

MEM_STATIC FORCE_INLINE_ATTR size_t ZSTD_extractHash(size_t hashAndTag, U32 tagBits, U32 hashBits) {
    (void)hashBits;
    return hashAndTag >> tagBits;
}

MEM_STATIC FORCE_INLINE_ATTR size_t ZSTD_extractIndex(size_t indexAndTag, U32 tagBits) {
    return ZSTD_extractHash(indexAndTag, tagBits, 0);
}

MEM_STATIC FORCE_INLINE_ATTR U32 ZSTD_extractTagFromHash(size_t hashAndTag, U32 tagBits, U32 hashBits) {
    (void)hashBits;
    return (U32)(hashAndTag & ((1 << tagBits)  - 1));
}

MEM_STATIC FORCE_INLINE_ATTR U32 ZSTD_extractTagFromIndex(size_t indexAndTag, U32 tagBits) {
    return ZSTD_extractTagFromHash(indexAndTag, tagBits, 0);
}

MEM_STATIC FORCE_INLINE_ATTR U32 ZSTD_combineIndexAndTag(U32 index, U32 tag, U32 tagBits) {
    assert(tag >> tagBits == 0);
    assert(index >> (32 - tagBits) == 0);
    return (index << tagBits) | tag;
}

#endif



/** ZSTD_ipow() :
 * Return base^exponent.
 */
static U64 ZSTD_ipow(U64 base, U64 exponent)
{
    U64 power = 1;
    while (exponent) {
      if (exponent & 1) power *= base;
      exponent >>= 1;
      base *= base;
    }
    return power;
}

#define ZSTD_ROLL_HASH_CHAR_OFFSET 10

/** ZSTD_rollingHash_append() :
 * Add the buffer to the hash value.
 */
static U64 ZSTD_rollingHash_append(U64 hash, void const* buf, size_t size)
{
    BYTE const* istart = (BYTE const*)buf;
    size_t pos;
    for (pos = 0; pos < size; ++pos) {
        hash *= prime8bytes;
        hash += istart[pos] + ZSTD_ROLL_HASH_CHAR_OFFSET;
    }
    return hash;
}

/** ZSTD_rollingHash_compute() :
 * Compute the rolling hash value of the buffer.
 */
MEM_STATIC U64 ZSTD_rollingHash_compute(void const* buf, size_t size)
{
    return ZSTD_rollingHash_append(0, buf, size);
}

/** ZSTD_rollingHash_primePower() :
 * Compute the primePower to be passed to ZSTD_rollingHash_rotate() for a hash
 * over a window of length bytes.
 */
MEM_STATIC U64 ZSTD_rollingHash_primePower(U32 length)
{
    return ZSTD_ipow(prime8bytes, length - 1);
}

/** ZSTD_rollingHash_rotate() :
 * Rotate the rolling hash by one byte.
 */
MEM_STATIC U64 ZSTD_rollingHash_rotate(U64 hash, BYTE toRemove, BYTE toAdd, U64 primePower)
{
    hash -= (toRemove + ZSTD_ROLL_HASH_CHAR_OFFSET) * primePower;
    hash *= prime8bytes;
    hash += toAdd + ZSTD_ROLL_HASH_CHAR_OFFSET;
    return hash;
}

/*-*************************************
*  Round buffer management
***************************************/
/* Max @current value allowed:
 * In 32-bit mode: we want to avoid crossing the 2 GB limit,
 *                 reducing risks of side effects in case of signed operations on indexes.
 * In 64-bit mode: we want to ensure that adding the maximum job size (512 MB)
 *                 doesn't overflow U32 index capacity (4 GB) */
#define ZSTD_CURRENT_MAX (MEM_64bits() ? 3500U MB : 2000U MB)
/* Maximum chunk size before overflow correction needs to be called again */
#define ZSTD_CHUNKSIZE_MAX                                                     \
    ( ((U32)-1)                  /* Maximum ending current index */            \
    - ZSTD_CURRENT_MAX)          /* Maximum beginning lowLimit */

/**
 * ZSTD_window_clear():
 * Clears the window containing the history by simply setting it to empty.
 */
MEM_STATIC void ZSTD_window_clear(ZSTD_window_t* window)
{
    size_t const endT = (size_t)(window->nextSrc - window->base);
    U32 const end = (U32)endT;

    window->lowLimit = end;
    window->dictLimit = end;
}

MEM_STATIC U32 ZSTD_window_isEmpty(ZSTD_window_t const window)
{
    return window.dictLimit == ZSTD_WINDOW_START_INDEX &&
           window.lowLimit == ZSTD_WINDOW_START_INDEX &&
           (window.nextSrc - window.base) == ZSTD_WINDOW_START_INDEX;
}

/**
 * ZSTD_window_hasExtDict():
 * Returns non-zero if the window has a non-empty extDict.
 */
MEM_STATIC U32 ZSTD_window_hasExtDict(ZSTD_window_t const window)
{
    return window.lowLimit < window.dictLimit;
}

/**
 * ZSTD_matchState_dictMode():
 * Inspects the provided matchState and figures out what dictMode should be
 * passed to the compressor.
 */
MEM_STATIC ZSTD_dictMode_e ZSTD_matchState_dictMode(const ZSTD_MatchState_t *ms)
{
    return ZSTD_window_hasExtDict(ms->window) ?
        ZSTD_extDict :
        ms->dictMatchState != NULL ?
            (ms->dictMatchState->dedicatedDictSearch ? ZSTD_dedicatedDictSearch : ZSTD_dictMatchState) :
            ZSTD_noDict;
}

/* Defining this macro to non-zero tells zstd to run the overflow correction
 * code much more frequently. This is very inefficient, and should only be
 * used for tests and fuzzers.
 */
#ifndef ZSTD_WINDOW_OVERFLOW_CORRECT_FREQUENTLY
#  ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#    define ZSTD_WINDOW_OVERFLOW_CORRECT_FREQUENTLY 1
#  else
#    define ZSTD_WINDOW_OVERFLOW_CORRECT_FREQUENTLY 0
#  endif
#endif

/**
 * ZSTD_window_canOverflowCorrect():
 * Returns non-zero if the indices are large enough for overflow correction
 * to work correctly without impacting compression ratio.
 */
MEM_STATIC U32 ZSTD_window_canOverflowCorrect(ZSTD_window_t const window,
                                              U32 cycleLog,
                                              U32 maxDist,
                                              U32 loadedDictEnd,
                                              void const* src)
{
    U32 const cycleSize = 1u << cycleLog;
    U32 const curr = (U32)((BYTE const*)src - window.base);
    U32 const minIndexToOverflowCorrect = cycleSize
                                        + MAX(maxDist, cycleSize)
                                        + ZSTD_WINDOW_START_INDEX;

    /* Adjust the min index to backoff the overflow correction frequency,
     * so we don't waste too much CPU in overflow correction. If this
     * computation overflows we don't really care, we just need to make
     * sure it is at least minIndexToOverflowCorrect.
     */
    U32 const adjustment = window.nbOverflowCorrections + 1;
    U32 const adjustedIndex = MAX(minIndexToOverflowCorrect * adjustment,
                                  minIndexToOverflowCorrect);
    U32 const indexLargeEnough = curr > adjustedIndex;

    /* Only overflow correct early if the dictionary is invalidated already,
     * so we don't hurt compression ratio.
     */
    U32 const dictionaryInvalidated = curr > maxDist + loadedDictEnd;

    return indexLargeEnough && dictionaryInvalidated;
}

/**
 * ZSTD_window_needOverflowCorrection():
 * Returns non-zero if the indices are getting too large and need overflow
 * protection.
 */
MEM_STATIC U32 ZSTD_window_needOverflowCorrection(ZSTD_window_t const window,
                                                  U32 cycleLog,
                                                  U32 maxDist,
                                                  U32 loadedDictEnd,
                                                  void const* src,
                                                  void const* srcEnd)
{
    U32 const curr = (U32)((BYTE const*)srcEnd - window.base);
    if (ZSTD_WINDOW_OVERFLOW_CORRECT_FREQUENTLY) {
        if (ZSTD_window_canOverflowCorrect(window, cycleLog, maxDist, loadedDictEnd, src)) {
            return 1;
        }
    }
    return curr > ZSTD_CURRENT_MAX;
}

/**
 * ZSTD_window_correctOverflow():
 * Reduces the indices to protect from index overflow.
 * Returns the correction made to the indices, which must be applied to every
 * stored index.
 *
 * The least significant cycleLog bits of the indices must remain the same,
 * which may be 0. Every index up to maxDist in the past must be valid.
 */
MEM_STATIC
ZSTD_ALLOW_POINTER_OVERFLOW_ATTR
U32 ZSTD_window_correctOverflow(ZSTD_window_t* window, U32 cycleLog,
                                           U32 maxDist, void const* src)
{
    /* preemptive overflow correction:
     * 1. correction is large enough:
     *    lowLimit > (3<<29) ==> current > 3<<29 + 1<<windowLog
     *    1<<windowLog <= newCurrent < 1<<chainLog + 1<<windowLog
     *
     *    current - newCurrent
     *    > (3<<29 + 1<<windowLog) - (1<<windowLog + 1<<chainLog)
     *    > (3<<29) - (1<<chainLog)
     *    > (3<<29) - (1<<30)             (NOTE: chainLog <= 30)
     *    > 1<<29
     *
     * 2. (ip+ZSTD_CHUNKSIZE_MAX - cctx->base) doesn't overflow:
     *    After correction, current is less than (1<<chainLog + 1<<windowLog).
     *    In 64-bit mode we are safe, because we have 64-bit ptrdiff_t.
     *    In 32-bit mode we are safe, because (chainLog <= 29), so
     *    ip+ZSTD_CHUNKSIZE_MAX - cctx->base < 1<<32.
     * 3. (cctx->lowLimit + 1<<windowLog) < 1<<32:
     *    windowLog <= 31 ==> 3<<29 + 1<<windowLog < 7<<29 < 1<<32.
     */
    U32 const cycleSize = 1u << cycleLog;
    U32 const cycleMask = cycleSize - 1;
    U32 const curr = (U32)((BYTE const*)src - window->base);
    U32 const currentCycle = curr & cycleMask;
    /* Ensure newCurrent - maxDist >= ZSTD_WINDOW_START_INDEX. */
    U32 const currentCycleCorrection = currentCycle < ZSTD_WINDOW_START_INDEX
                                     ? MAX(cycleSize, ZSTD_WINDOW_START_INDEX)
                                     : 0;
    U32 const newCurrent = currentCycle
                         + currentCycleCorrection
                         + MAX(maxDist, cycleSize);
    U32 const correction = curr - newCurrent;
    /* maxDist must be a power of two so that:
     *   (newCurrent & cycleMask) == (curr & cycleMask)
     * This is required to not corrupt the chains / binary tree.
     */
    assert((maxDist & (maxDist - 1)) == 0);
    assert((curr & cycleMask) == (newCurrent & cycleMask));
    assert(curr > newCurrent);
    if (!ZSTD_WINDOW_OVERFLOW_CORRECT_FREQUENTLY) {
        /* Loose bound, should be around 1<<29 (see above) */
        assert(correction > 1<<28);
    }

    window->base += correction;
    window->dictBase += correction;
    if (window->lowLimit < correction + ZSTD_WINDOW_START_INDEX) {
        window->lowLimit = ZSTD_WINDOW_START_INDEX;
    } else {
        window->lowLimit -= correction;
    }
    if (window->dictLimit < correction + ZSTD_WINDOW_START_INDEX) {
        window->dictLimit = ZSTD_WINDOW_START_INDEX;
    } else {
        window->dictLimit -= correction;
    }

    /* Ensure we can still reference the full window. */
    assert(newCurrent >= maxDist);
    assert(newCurrent - maxDist >= ZSTD_WINDOW_START_INDEX);
    /* Ensure that lowLimit and dictLimit didn't underflow. */
    assert(window->lowLimit <= newCurrent);
    assert(window->dictLimit <= newCurrent);

    ++window->nbOverflowCorrections;

    DEBUGLOG(4, "Correction of 0x%x bytes to lowLimit=0x%x", correction,
             window->lowLimit);
    return correction;
}

/**
 * ZSTD_window_enforceMaxDist():
 * Updates lowLimit so that:
 *    (srcEnd - base) - lowLimit == maxDist + loadedDictEnd
 *
 * It ensures index is valid as long as index >= lowLimit.
 * This must be called before a block compression call.
 *
 * loadedDictEnd is only defined if a dictionary is in use for current compression.
 * As the name implies, loadedDictEnd represents the index at end of dictionary.
 * The value lies within context's referential, it can be directly compared to blockEndIdx.
 *
 * If loadedDictEndPtr is NULL, no dictionary is in use, and we use loadedDictEnd == 0.
 * If loadedDictEndPtr is not NULL, we set it to zero after updating lowLimit.
 * This is because dictionaries are allowed to be referenced fully
 * as long as the last byte of the dictionary is in the window.
 * Once input has progressed beyond window size, dictionary cannot be referenced anymore.
 *
 * In normal dict mode, the dictionary lies between lowLimit and dictLimit.
 * In dictMatchState mode, lowLimit and dictLimit are the same,
 * and the dictionary is below them.
 * forceWindow and dictMatchState are therefore incompatible.
 */
MEM_STATIC void
ZSTD_window_enforceMaxDist(ZSTD_window_t* window,
                     const void* blockEnd,
                           U32   maxDist,
                           U32*  loadedDictEndPtr,
                     const ZSTD_MatchState_t** dictMatchStatePtr)
{
    U32 const blockEndIdx = (U32)((BYTE const*)blockEnd - window->base);
    U32 const loadedDictEnd = (loadedDictEndPtr != NULL) ? *loadedDictEndPtr : 0;
    DEBUGLOG(5, "ZSTD_window_enforceMaxDist: blockEndIdx=%u, maxDist=%u, loadedDictEnd=%u",
                (unsigned)blockEndIdx, (unsigned)maxDist, (unsigned)loadedDictEnd);

    /* - When there is no dictionary : loadedDictEnd == 0.
         In which case, the test (blockEndIdx > maxDist) is merely to avoid
         overflowing next operation `newLowLimit = blockEndIdx - maxDist`.
       - When there is a standard dictionary :
         Index referential is copied from the dictionary,
         which means it starts from 0.
         In which case, loadedDictEnd == dictSize,
         and it makes sense to compare `blockEndIdx > maxDist + dictSize`
         since `blockEndIdx` also starts from zero.
       - When there is an attached dictionary :
         loadedDictEnd is expressed within the referential of the context,
         so it can be directly compared against blockEndIdx.
    */
    if (blockEndIdx > maxDist + loadedDictEnd) {
        U32 const newLowLimit = blockEndIdx - maxDist;
        if (window->lowLimit < newLowLimit) window->lowLimit = newLowLimit;
        if (window->dictLimit < window->lowLimit) {
            DEBUGLOG(5, "Update dictLimit to match lowLimit, from %u to %u",
                        (unsigned)window->dictLimit, (unsigned)window->lowLimit);
            window->dictLimit = window->lowLimit;
        }
        /* On reaching window size, dictionaries are invalidated */
        if (loadedDictEndPtr) *loadedDictEndPtr = 0;
        if (dictMatchStatePtr) *dictMatchStatePtr = NULL;
    }
}

/* Similar to ZSTD_window_enforceMaxDist(),
 * but only invalidates dictionary
 * when input progresses beyond window size.
 * assumption : loadedDictEndPtr and dictMatchStatePtr are valid (non NULL)
 *              loadedDictEnd uses same referential as window->base
 *              maxDist is the window size */
MEM_STATIC void
ZSTD_checkDictValidity(const ZSTD_window_t* window,
                       const void* blockEnd,
                             U32   maxDist,
                             U32*  loadedDictEndPtr,
                       const ZSTD_MatchState_t** dictMatchStatePtr)
{
    assert(loadedDictEndPtr != NULL);
    assert(dictMatchStatePtr != NULL);
    {   U32 const blockEndIdx = (U32)((BYTE const*)blockEnd - window->base);
        U32 const loadedDictEnd = *loadedDictEndPtr;
        DEBUGLOG(5, "ZSTD_checkDictValidity: blockEndIdx=%u, maxDist=%u, loadedDictEnd=%u",
                    (unsigned)blockEndIdx, (unsigned)maxDist, (unsigned)loadedDictEnd);
        assert(blockEndIdx >= loadedDictEnd);

        if (blockEndIdx > loadedDictEnd + maxDist || loadedDictEnd != window->dictLimit) {
            /* On reaching window size, dictionaries are invalidated.
             * For simplification, if window size is reached anywhere within next block,
             * the dictionary is invalidated for the full block.
             *
             * We also have to invalidate the dictionary if ZSTD_window_update() has detected
             * non-contiguous segments, which means that loadedDictEnd != window->dictLimit.
             * loadedDictEnd may be 0, if forceWindow is true, but in that case we never use
             * dictMatchState, so setting it to NULL is not a problem.
             */
            DEBUGLOG(6, "invalidating dictionary for current block (distance > windowSize)");
            *loadedDictEndPtr = 0;
            *dictMatchStatePtr = NULL;
        } else {
            if (*loadedDictEndPtr != 0) {
                DEBUGLOG(6, "dictionary considered valid for current block");
    }   }   }
}

MEM_STATIC void ZSTD_window_init(ZSTD_window_t* window) {
    ZSTD_memset(window, 0, sizeof(*window));
    window->base = (BYTE const*)" ";
    window->dictBase = (BYTE const*)" ";
    ZSTD_STATIC_ASSERT(ZSTD_DUBT_UNSORTED_MARK < ZSTD_WINDOW_START_INDEX); /* Start above ZSTD_DUBT_UNSORTED_MARK */
    window->dictLimit = ZSTD_WINDOW_START_INDEX;    /* start from >0, so that 1st position is valid */
    window->lowLimit = ZSTD_WINDOW_START_INDEX;     /* it ensures first and later CCtx usages compress the same */
    window->nextSrc = window->base + ZSTD_WINDOW_START_INDEX;   /* see issue #1241 */
    window->nbOverflowCorrections = 0;
}

/**
 * ZSTD_window_update():
 * Updates the window by appending [src, src + srcSize) to the window.
 * If it is not contiguous, the current prefix becomes the extDict, and we
 * forget about the extDict. Handles overlap of the prefix and extDict.
 * Returns non-zero if the segment is contiguous.
 */
MEM_STATIC
ZSTD_ALLOW_POINTER_OVERFLOW_ATTR
U32 ZSTD_window_update(ZSTD_window_t* window,
                 const void* src, size_t srcSize,
                       int forceNonContiguous)
{
    BYTE const* const ip = (BYTE const*)src;
    U32 contiguous = 1;
    DEBUGLOG(5, "ZSTD_window_update");
    if (srcSize == 0)
        return contiguous;
    assert(window->base != NULL);
    assert(window->dictBase != NULL);
    /* Check if blocks follow each other */
    if (src != window->nextSrc || forceNonContiguous) {
        /* not contiguous */
        size_t const distanceFromBase = (size_t)(window->nextSrc - window->base);
        DEBUGLOG(5, "Non contiguous blocks, new segment starts at %u", window->dictLimit);
        window->lowLimit = window->dictLimit;
        assert(distanceFromBase == (size_t)(U32)distanceFromBase);  /* should never overflow */
        window->dictLimit = (U32)distanceFromBase;
        window->dictBase = window->base;
        window->base = ip - distanceFromBase;
        /* ms->nextToUpdate = window->dictLimit; */
        if (window->dictLimit - window->lowLimit < HASH_READ_SIZE) window->lowLimit = window->dictLimit;   /* too small extDict */
        contiguous = 0;
    }
    window->nextSrc = ip + srcSize;
    /* if input and dictionary overlap : reduce dictionary (area presumed modified by input) */
    if ( (ip+srcSize > window->dictBase + window->lowLimit)
       & (ip < window->dictBase + window->dictLimit)) {
        size_t const highInputIdx = (size_t)((ip + srcSize) - window->dictBase);
        U32 const lowLimitMax = (highInputIdx > (size_t)window->dictLimit) ? window->dictLimit : (U32)highInputIdx;
        assert(highInputIdx < UINT_MAX);
        window->lowLimit = lowLimitMax;
        DEBUGLOG(5, "Overlapping extDict and input : new lowLimit = %u", window->lowLimit);
    }
    return contiguous;
}

/**
 * Returns the lowest allowed match index. It may either be in the ext-dict or the prefix.
 */
MEM_STATIC U32 ZSTD_getLowestMatchIndex(const ZSTD_MatchState_t* ms, U32 curr, unsigned windowLog)
{
    U32 const maxDistance = 1U << windowLog;
    U32 const lowestValid = ms->window.lowLimit;
    U32 const withinWindow = (curr - lowestValid > maxDistance) ? curr - maxDistance : lowestValid;
    U32 const isDictionary = (ms->loadedDictEnd != 0);
    /* When using a dictionary the entire dictionary is valid if a single byte of the dictionary
     * is within the window. We invalidate the dictionary (and set loadedDictEnd to 0) when it isn't
     * valid for the entire block. So this check is sufficient to find the lowest valid match index.
     */
    U32 const matchLowest = isDictionary ? lowestValid : withinWindow;
    return matchLowest;
}

/**
 * Returns the lowest allowed match index in the prefix.
 */
MEM_STATIC U32 ZSTD_getLowestPrefixIndex(const ZSTD_MatchState_t* ms, U32 curr, unsigned windowLog)
{
    U32    const maxDistance = 1U << windowLog;
    U32    const lowestValid = ms->window.dictLimit;
    U32    const withinWindow = (curr - lowestValid > maxDistance) ? curr - maxDistance : lowestValid;
    U32    const isDictionary = (ms->loadedDictEnd != 0);
    /* When computing the lowest prefix index we need to take the dictionary into account to handle
     * the edge case where the dictionary and the source are contiguous in memory.
     */
    U32    const matchLowest = isDictionary ? lowestValid : withinWindow;
    return matchLowest;
}

/* index_safety_check:
 * intentional underflow : ensure repIndex isn't overlapping dict + prefix
 * @return 1 if values are not overlapping,
 * 0 otherwise */
MEM_STATIC int ZSTD_index_overlap_check(const U32 prefixLowestIndex, const U32 repIndex) {
    return ((U32)((prefixLowestIndex-1)  - repIndex) >= 3);
}


/* debug functions */
#if (DEBUGLEVEL>=2)

MEM_STATIC double ZSTD_fWeight(U32 rawStat)
{
    U32 const fp_accuracy = 8;
    U32 const fp_multiplier = (1 << fp_accuracy);
    U32 const newStat = rawStat + 1;
    U32 const hb = ZSTD_highbit32(newStat);
    U32 const BWeight = hb * fp_multiplier;
    U32 const FWeight = (newStat << fp_accuracy) >> hb;
    U32 const weight = BWeight + FWeight;
    assert(hb + fp_accuracy < 31);
    return (double)weight / fp_multiplier;
}

/* display a table content,
 * listing each element, its frequency, and its predicted bit cost */
MEM_STATIC void ZSTD_debugTable(const U32* table, U32 max)
{
    unsigned u, sum;
    for (u=0, sum=0; u<=max; u++) sum += table[u];
    DEBUGLOG(2, "total nb elts: %u", sum);
    for (u=0; u<=max; u++) {
        DEBUGLOG(2, "%2u: %5u  (%.2f)",
                u, table[u], ZSTD_fWeight(sum) - ZSTD_fWeight(table[u]) );
    }
}

#endif

/* Short Cache */

/* Normally, zstd matchfinders follow this flow:
 *     1. Compute hash at ip
 *     2. Load index from hashTable[hash]
 *     3. Check if *ip == *(base + index)
 * In dictionary compression, loading *(base + index) is often an L2 or even L3 miss.
 *
 * Short cache is an optimization which allows us to avoid step 3 most of the time
 * when the data doesn't actually match. With short cache, the flow becomes:
 *     1. Compute (hash, currentTag) at ip. currentTag is an 8-bit independent hash at ip.
 *     2. Load (index, matchTag) from hashTable[hash]. See ZSTD_writeTaggedIndex to understand how this works.
 *     3. Only if currentTag == matchTag, check *ip == *(base + index). Otherwise, continue.
 *
 * Currently, short cache is only implemented in CDict hashtables. Thus, its use is limited to
 * dictMatchState matchfinders.
 */
#define ZSTD_SHORT_CACHE_TAG_BITS 8
#define ZSTD_SHORT_CACHE_TAG_MASK ((1u << ZSTD_SHORT_CACHE_TAG_BITS) - 1)

/* Helper function for ZSTD_fillHashTable and ZSTD_fillDoubleHashTable.
 * Unpacks hashAndTag into (hash, tag), then packs (index, tag) into hashTable[hash]. */
MEM_STATIC void ZSTD_writeTaggedIndex(U32* const hashTable, size_t hashAndTag, U32 const hashLog, U32 index) {
    size_t const hash = ZSTD_extractHash(hashAndTag, ZSTD_SHORT_CACHE_TAG_BITS, hashLog);
    U32 const tag = ZSTD_extractTagFromHash(hashAndTag, ZSTD_SHORT_CACHE_TAG_BITS, hashLog);
    assert(index >> (32 - ZSTD_SHORT_CACHE_TAG_BITS) == 0);
    hashTable[hash] = ZSTD_combineIndexAndTag(index, tag, ZSTD_SHORT_CACHE_TAG_BITS);
}

/* Helper function for short cache matchfinders.
 * Unpacks tag1 and tag2 from packedIndexTag1 and packedHashTag2, then checks if the tags match. */
MEM_STATIC int ZSTD_comparePackedTags(size_t packedIndexTag1, size_t packedHashTag2, U32 const hashLog) {
    U32 const tag1 = ZSTD_extractTagFromIndex(packedIndexTag1, ZSTD_SHORT_CACHE_TAG_BITS);
    U32 const tag2 = ZSTD_extractTagFromHash(packedHashTag2, ZSTD_SHORT_CACHE_TAG_BITS, hashLog);
    return tag1 == tag2;
}

/* ===============================================================
 * Shared internal declarations
 * These prototypes may be called from sources not in lib/compress
 * =============================================================== */

/* ZSTD_loadCEntropy() :
 * dict : must point at beginning of a valid zstd dictionary.
 * return : size of dictionary header (size of magic number + dict ID + entropy tables)
 * assumptions : magic number supposed already checked
 *               and dictSize >= 8 */
size_t ZSTD_loadCEntropy(ZSTD_compressedBlockState_t* bs, void* workspace,
                         const void* const dict, size_t dictSize);

void ZSTD_reset_compressedBlockState(ZSTD_compressedBlockState_t* bs);

typedef struct {
    U32 idx;            /* Index in array of ZSTD_Sequence */
    U32 posInSequence;  /* Position within sequence at idx */
    size_t posInSrc;    /* Number of bytes given by sequences provided so far */
} ZSTD_SequencePosition;

/* for benchmark */
size_t ZSTD_convertBlockSequences(ZSTD_CCtx* cctx,
                        const ZSTD_Sequence* const inSeqs, size_t nbSequences,
                        int repcodeResolution);

typedef struct {
    size_t nbSequences;
    size_t blockSize;
    size_t litSize;
} BlockSummary;

BlockSummary ZSTD_get1BlockSummary(const ZSTD_Sequence* seqs, size_t nbSeqs);

/* ==============================================================
 * Private declarations
 * These prototypes shall only be called from within lib/compress
 * ============================================================== */

/* ZSTD_getCParamsFromCCtxParams() :
 * cParams are built depending on compressionLevel, src size hints,
 * LDM and manually set compression parameters.
 * Note: srcSizeHint == 0 means 0!
 */
ZSTD_compressionParameters ZSTD_getCParamsFromCCtxParams(
        const ZSTD_CCtx_params* CCtxParams, U64 srcSizeHint, size_t dictSize, ZSTD_CParamMode_e mode);

/*! ZSTD_initCStream_internal() :
 *  Private use only. Init streaming operation.
 *  expects params to be valid.
 *  must receive dict, or cdict, or none, but not both.
 *  @return : 0, or an error code */
size_t ZSTD_initCStream_internal(ZSTD_CStream* zcs,
                     const void* dict, size_t dictSize,
                     const ZSTD_CDict* cdict,
                     const ZSTD_CCtx_params* params, unsigned long long pledgedSrcSize);

void ZSTD_resetSeqStore(SeqStore_t* ssPtr);

/*! ZSTD_getCParamsFromCDict() :
 *  as the name implies */
ZSTD_compressionParameters ZSTD_getCParamsFromCDict(const ZSTD_CDict* cdict);

/* ZSTD_compressBegin_advanced_internal() :
 * Private use only. To be called from zstdmt_compress.c. */
size_t ZSTD_compressBegin_advanced_internal(ZSTD_CCtx* cctx,
                                    const void* dict, size_t dictSize,
                                    ZSTD_dictContentType_e dictContentType,
                                    ZSTD_dictTableLoadMethod_e dtlm,
                                    const ZSTD_CDict* cdict,
                                    const ZSTD_CCtx_params* params,
                                    unsigned long long pledgedSrcSize);

/* ZSTD_compress_advanced_internal() :
 * Private use only. To be called from zstdmt_compress.c. */
size_t ZSTD_compress_advanced_internal(ZSTD_CCtx* cctx,
                                       void* dst, size_t dstCapacity,
                                 const void* src, size_t srcSize,
                                 const void* dict,size_t dictSize,
                                 const ZSTD_CCtx_params* params);


/* ZSTD_writeLastEmptyBlock() :
 * output an empty Block with end-of-frame mark to complete a frame
 * @return : size of data written into `dst` (== ZSTD_blockHeaderSize (defined in zstd_internal.h))
 *           or an error code if `dstCapacity` is too small (<ZSTD_blockHeaderSize)
 */
size_t ZSTD_writeLastEmptyBlock(void* dst, size_t dstCapacity);


/* ZSTD_referenceExternalSequences() :
 * Must be called before starting a compression operation.
 * seqs must parse a prefix of the source.
 * This cannot be used when long range matching is enabled.
 * Zstd will use these sequences, and pass the literals to a secondary block
 * compressor.
 * NOTE: seqs are not verified! Invalid sequences can cause out-of-bounds memory
 * access and data corruption.
 */
void ZSTD_referenceExternalSequences(ZSTD_CCtx* cctx, rawSeq* seq, size_t nbSeq);

/** ZSTD_cycleLog() :
 *  condition for correct operation : hashLog > 1 */
U32 ZSTD_cycleLog(U32 hashLog, ZSTD_strategy strat);

/** ZSTD_CCtx_trace() :
 *  Trace the end of a compression call.
 */
void ZSTD_CCtx_trace(ZSTD_CCtx* cctx, size_t extraCSize);

/* Returns 1 if an external sequence producer is registered, otherwise returns 0. */
MEM_STATIC int ZSTD_hasExtSeqProd(const ZSTD_CCtx_params* params) {
    return params->extSeqProdFunc != NULL;
}

/* ===============================================================
 * Deprecated definitions that are still used internally to avoid
 * deprecation warnings. These functions are exactly equivalent to
 * their public variants, but avoid the deprecation warnings.
 * =============================================================== */

size_t ZSTD_compressBegin_usingCDict_deprecated(ZSTD_CCtx* cctx, const ZSTD_CDict* cdict);

size_t ZSTD_compressContinue_public(ZSTD_CCtx* cctx,
                                    void* dst, size_t dstCapacity,
                              const void* src, size_t srcSize);

size_t ZSTD_compressEnd_public(ZSTD_CCtx* cctx,
                               void* dst, size_t dstCapacity,
                         const void* src, size_t srcSize);

size_t ZSTD_compressBlock_deprecated(ZSTD_CCtx* cctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);


#endif /* ZSTD_COMPRESS_H */
