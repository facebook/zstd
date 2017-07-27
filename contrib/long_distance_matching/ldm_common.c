#include <stdio.h>

#include "ldm.h"

/**
 * This function reads the header at the beginning of src and writes
 * the compressed and decompressed size to compressedSize and
 * decompressedSize.
 *
 * The header consists of 16 bytes: 8 bytes each in little-endian format
 * of the compressed size and the decompressed size.
 */
void LDM_readHeader(const void *src, U64 *compressedSize,
                    U64 *decompressedSize) {
  const BYTE *ip = (const BYTE *)src;
  *compressedSize = MEM_readLE64(ip);
  *decompressedSize = MEM_readLE64(ip + 8);
}

/**
 * Writes the 16-byte header (8-bytes each of the compressedSize and
 * decompressedSize in little-endian format) to memPtr.
 */
void LDM_writeHeader(void *memPtr, U64 compressedSize,
                     U64 decompressedSize) {
  MEM_writeLE64(memPtr, compressedSize);
  MEM_writeLE64((BYTE *)memPtr + 8, decompressedSize);
}

struct LDM_DCtx {
  size_t compressedSize;
  size_t maxDecompressedSize;

  const BYTE *ibase;   /* Base of input */
  const BYTE *ip;      /* Current input position */
  const BYTE *iend;    /* End of source */

  const BYTE *obase;   /* Base of output */
  BYTE *op;            /* Current output position */
  const BYTE *oend;    /* End of output */
};

void LDM_initializeDCtx(LDM_DCtx *dctx,
                        const void *src, size_t compressedSize,
                        void *dst, size_t maxDecompressedSize) {
  dctx->compressedSize = compressedSize;
  dctx->maxDecompressedSize = maxDecompressedSize;

  dctx->ibase = src;
  dctx->ip = (const BYTE *)src;
  dctx->iend = dctx->ip + dctx->compressedSize;
  dctx->op = dst;
  dctx->oend = dctx->op + dctx->maxDecompressedSize;
}

size_t LDM_decompress(const void *src, size_t compressedSize,
                      void *dst, size_t maxDecompressedSize) {

  LDM_DCtx dctx;
  LDM_initializeDCtx(&dctx, src, compressedSize, dst, maxDecompressedSize);

  while (dctx.ip < dctx.iend) {
    BYTE *cpy;
    const BYTE *match;
    size_t length, offset;

    /* Get the literal length. */
    const unsigned token = *(dctx.ip)++;
    if ((length = (token >> ML_BITS)) == RUN_MASK) {
      unsigned s;
      do {
        s = *(dctx.ip)++;
        length += s;
      } while (s == 255);
    }

    /* Copy the literals. */
    cpy = dctx.op + length;
    memcpy(dctx.op, dctx.ip, length);
    dctx.ip += length;
    dctx.op = cpy;

    //TODO: dynamic offset size?
    /* Encode the offset. */
    offset = MEM_read32(dctx.ip);
    dctx.ip += LDM_OFFSET_SIZE;
    match = dctx.op - offset;

    /* Get the match length. */
    length = token & ML_MASK;
    if (length == ML_MASK) {
      unsigned s;
      do {
        s = *(dctx.ip)++;
        length += s;
      } while (s == 255);
    }
    length += LDM_MIN_MATCH_LENGTH;

    /* Copy match. */
    cpy = dctx.op + length;

    // TODO: this can be made more efficient.
    while (match < cpy - offset && dctx.op < dctx.oend) {
      *(dctx.op)++ = *match++;
    }
  }
  return dctx.op - (BYTE *)dst;
}
