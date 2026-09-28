/* ******************************************************************
 * zstd_file - File access convenience API
 *
 * zlib-style file convenience wrappers (gzopen / gzread / gzwrite spirit):
 * compress, buffer, and interact with the filesystem through stdio.
 *
 * This is purely additive: it sits on top of the public streaming API
 * (ZSTD_compressStream2 / ZSTD_decompressStream, declared in ../zstd.h)
 * plus stdio, and produces/consumes plain zstd frames. Files written
 * here are readable by the `zstd` command line tool and any other
 * zstd decoder, and vice versa.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 ****************************************************************** */

#include <stdio.h>   /* FILE, fopen, fread, fwrite, fclose, fflush */
#include <stdlib.h>  /* malloc, calloc, free */
#include <string.h>  /* memcpy */
#include <errno.h>   /* errno, EINVAL, ENOMEM */
#include "../zstd.h" /* public API, incl. ZSTD_fopen() family */

/* Opaque context, as declared in zstd.h */
struct ZSTD_FCtx_s {
    FILE* file;
    int writeMode;             /* 1 = compressing on write, 0 = decompressing on read */
    int eof;                   /* read side reached end of file (not an error) */
    ZSTD_ErrorCode errCode;    /* sticky error state, ZSTD_error_no_error when fine */
    ZSTD_CStream* cstream;     /* write side only */
    ZSTD_DStream* dstream;     /* read side only */
    char*  wBuf;               /* write side compressed-output staging */
    size_t wSize;
    char*  inBuf;              /* read side compressed-input staging */
    size_t inSize;
    size_t inPos;
    size_t inFill;
    char*  outBuf;             /* read side decompressed-output staging */
    size_t outSize;
    size_t outPos;
    size_t outFill;
};


/* @return 1 for write mode, 0 for read mode, -1 for invalid mode string.
 * Accepted modes contain exactly one of 'r' / 'w',
 * with optional 'b' and 't' characters which are accepted and ignored
 * (all I/O is binary). Anything else (including append modes) is invalid. */
static int ZSTD_parseFileMode(const char* mode)
{
    int hasR = 0;
    int hasW = 0;
    if (mode == NULL) return -1;
    for (; *mode != '\0'; ++mode) {
        switch (*mode) {
            case 'r': hasR = 1; break;
            case 'w': hasW = 1; break;
            case 'b':           /* accepted, ignored */
            case 't':           /* accepted, ignored */
                break;
            default:
                return -1;
        }
    }
    if (hasR == hasW) return -1;  /* exactly one of 'r' / 'w' required */
    return hasW;
}

/* Note: write side uses ZSTD_CLEVEL_DEFAULT when compressionLevel == 0.
 * Out-of-range levels are clamped to [ZSTD_minCLevel(), ZSTD_maxCLevel()]. */
ZSTD_FCtx* ZSTD_fopenLevel(const char* path, const char* mode, int compressionLevel)
{
    int writeMode = ZSTD_parseFileMode(mode);
    FILE* fp;
    ZSTD_FCtx* f;
    int savedErrno;

    if (writeMode < 0 || path == NULL) {
        errno = EINVAL;
        return NULL;
    }
    fp = fopen(path, writeMode ? "wb" : "rb");
    if (fp == NULL) return NULL;  /* errno preserved from fopen() */

    f = (ZSTD_FCtx*)calloc(1, sizeof(*f));
    if (f == NULL) {
        savedErrno = errno;
        fclose(fp);
        errno = savedErrno != 0 ? savedErrno : ENOMEM;
        return NULL;
    }
    f->file = fp;
    f->writeMode = writeMode;
    f->errCode = ZSTD_error_no_error;

    if (writeMode) {
        size_t r;
        if (compressionLevel == 0) compressionLevel = ZSTD_CLEVEL_DEFAULT;
        if (compressionLevel < ZSTD_minCLevel()) compressionLevel = ZSTD_minCLevel();
        if (compressionLevel > ZSTD_maxCLevel()) compressionLevel = ZSTD_maxCLevel();
        f->cstream = ZSTD_createCStream();
        if (f->cstream == NULL) goto _open_failed;
        r = ZSTD_CCtx_reset(f->cstream, ZSTD_reset_session_and_parameters);
        if (ZSTD_isError(r)) goto _open_failed;
        r = ZSTD_CCtx_setParameter(f->cstream, ZSTD_c_compressionLevel, compressionLevel);
        if (ZSTD_isError(r)) goto _open_failed;
        f->wSize = ZSTD_CStreamOutSize();
        f->wBuf = (char*)malloc(f->wSize);
        if (f->wBuf == NULL) goto _open_failed;
    } else {
        f->dstream = ZSTD_createDStream();
        if (f->dstream == NULL) goto _open_failed;
        f->inSize = ZSTD_DStreamInSize();
        f->inBuf = (char*)malloc(f->inSize);
        if (f->inBuf == NULL) goto _open_failed;
        f->outSize = ZSTD_DStreamOutSize();
        f->outBuf = (char*)malloc(f->outSize);
        if (f->outBuf == NULL) goto _open_failed;
    }
    return f;

_open_failed:
    savedErrno = errno;
    if (f->cstream != NULL) ZSTD_freeCStream(f->cstream);
    if (f->dstream != NULL) ZSTD_freeDStream(f->dstream);
    free(f->wBuf);
    free(f->inBuf);
    free(f->outBuf);
    fclose(fp);
    free(f);
    errno = savedErrno != 0 ? savedErrno : ENOMEM;
    return NULL;
}

ZSTD_FCtx* ZSTD_fopen(const char* path, const char* mode)
{
    return ZSTD_fopenLevel(path, mode, 0);
}

/* Write-side helper: push staged compressed bytes to the file.
 * @return 0 on success, non-zero ZSTD_ErrorCode value on failure. */
static int ZSTD_fPushOutput(ZSTD_FCtx* f, const char* buf, size_t size)
{
    if (size == 0) return 0;
    if (fwrite(buf, 1, size, f->file) != size) {
        f->errCode = ZSTD_error_GENERIC;  /* errno preserved from fwrite() */
        return (int)ZSTD_error_GENERIC;
    }
    return 0;
}

size_t ZSTD_fwrite(const void* src, size_t size, ZSTD_FCtx* f)
{
    ZSTD_inBuffer in;
    if (size == 0) return 0;
    if (f == NULL || src == NULL) return 0;
    if (!f->writeMode || f->errCode != ZSTD_error_no_error) {
        if (f->errCode == ZSTD_error_no_error) f->errCode = ZSTD_error_GENERIC;
        return 0;
    }
    in.src = src;
    in.size = size;
    in.pos = 0;
    while (in.pos < in.size) {
        ZSTD_outBuffer out;
        size_t r;
        out.dst = f->wBuf;
        out.size = f->wSize;
        out.pos = 0;
        r = ZSTD_compressStream2(f->cstream, &out, &in, ZSTD_e_continue);
        if (ZSTD_isError(r)) {
            f->errCode = ZSTD_getErrorCode(r);
            return 0;
        }
        if (ZSTD_fPushOutput(f, f->wBuf, out.pos) != 0) return 0;
    }
    return size;
}

size_t ZSTD_fread(void* dst, size_t size, ZSTD_FCtx* f)
{
    char* op;
    size_t delivered = 0;
    if (size == 0) return 0;
    if (f == NULL || dst == NULL) return 0;
    if (f->writeMode || f->errCode != ZSTD_error_no_error) {
        if (f->errCode == ZSTD_error_no_error) f->errCode = ZSTD_error_GENERIC;
        return 0;
    }
    op = (char*)dst;
    while (delivered < size && !f->eof) {
        /* serve from staged decompressed output first */
        if (f->outPos < f->outFill) {
            size_t staged = f->outFill - f->outPos;
            size_t want = size - delivered;
            size_t chunk = staged < want ? staged : want;
            memcpy(op + delivered, f->outBuf + f->outPos, chunk);
            f->outPos += chunk;
            delivered += chunk;
            continue;
        }
        /* refill compressed input when exhausted */
        if (f->inPos == f->inFill) {
            f->inFill = fread(f->inBuf, 1, f->inSize, f->file);
            f->inPos = 0;
            if (f->inFill == 0) {
                if (ferror(f->file)) f->errCode = ZSTD_error_GENERIC;  /* errno preserved */
                else f->eof = 1;
                break;
            }
        }
        {   /* decode one step into the staging buffer */
            ZSTD_inBuffer in;
            ZSTD_outBuffer out;
            size_t r;
            in.src = f->inBuf;
            in.size = f->inFill;
            in.pos = f->inPos;
            out.dst = f->outBuf;
            out.size = f->outSize;
            out.pos = 0;
            r = ZSTD_decompressStream(f->dstream, &out, &in);
            f->inPos = in.pos;
            if (ZSTD_isError(r)) {
                f->errCode = ZSTD_getErrorCode(r);
                break;
            }
            f->outFill = out.pos;
            f->outPos = 0;
            /* r == 0 means current frame fully decoded and flushed;
             * the loop continues into a following frame when more input
             * exists (concatenated frames), or reaches EOF otherwise. */
        }
    }
    return delivered;
}

int ZSTD_ferror(const ZSTD_FCtx* f)
{
    if (f == NULL) return (int)ZSTD_error_GENERIC;
    return (int)f->errCode;
}

int ZSTD_fclose(ZSTD_FCtx* f)
{
    int ret;
    if (f == NULL) return 0;
    if (f->writeMode && f->errCode == ZSTD_error_no_error) {
        /* flush pending input and complete the frame */
        ZSTD_inBuffer in;
        size_t remaining;
        in.src = NULL;
        in.size = 0;
        in.pos = 0;
        do {
            ZSTD_outBuffer out;
            out.dst = f->wBuf;
            out.size = f->wSize;
            out.pos = 0;
            remaining = ZSTD_compressStream2(f->cstream, &out, &in, ZSTD_e_end);
            if (ZSTD_isError(remaining)) {
                f->errCode = ZSTD_getErrorCode(remaining);
                break;
            }
            if (ZSTD_fPushOutput(f, f->wBuf, out.pos) != 0) break;
        } while (remaining != 0);
    }
    /* The FILE is closed and the handle freed even on error. */
    if (f->errCode == ZSTD_error_no_error) {
        if (fflush(f->file) != 0) f->errCode = ZSTD_error_GENERIC;  /* errno preserved */
    }
    if (fclose(f->file) != 0 && f->errCode == ZSTD_error_no_error) {
        f->errCode = ZSTD_error_GENERIC;  /* errno preserved */
    }
    ret = (int)f->errCode;
    if (f->cstream != NULL) ZSTD_freeCStream(f->cstream);
    if (f->dstream != NULL) ZSTD_freeDStream(f->dstream);
    free(f->wBuf);
    free(f->inBuf);
    free(f->outBuf);
    free(f);
    return ret;
}
