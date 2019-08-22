/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "zstd_cwksp.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * Align must be a power of 2.
 */
size_t ZSTD_cwksp_align(size_t size, size_t const align) {
    size_t const mask = align - 1;
    assert((align & mask) == 0);
    return (size + mask) & ~mask;
}

/**
 * Internal function, use wrappers instead.
 */
static void* ZSTD_cwksp_reserve_internal(ZSTD_cwksp* ws, size_t bytes, ZSTD_cwksp_alloc_phase_e phase) {
    /* TODO(felixh): alignment */
    void* alloc = (BYTE *)ws->allocStart - bytes;
    void* bottom = ws->tableEnd;
    DEBUGLOG(3, "wksp: reserving align %zd bytes, %zd bytes remaining",
        bytes, (BYTE *)alloc - (BYTE *)bottom);
    assert(phase >= ws->phase);
    if (phase > ws->phase) {
        if (ws->phase < ZSTD_cwksp_alloc_buffers &&
                phase >= ZSTD_cwksp_alloc_buffers) {
        }
        if (ws->phase < ZSTD_cwksp_alloc_aligned &&
                phase >= ZSTD_cwksp_alloc_aligned) {
            /* If unaligned allocations down from a too-large top have left us
             * unaligned, we need to realign our alloc ptr. Technically, this
             * can consume space that is unaccounted for in the neededSpace
             * calculation. However, I believe this can only happen when the
             * workspace is too large, and specifically when it is too large
             * by a larger margin than the space that will be consumed. */
            /* TODO: cleaner, compiler warning friendly way to do this??? */
            alloc = (BYTE*)alloc - ((size_t)alloc & (sizeof(U32)-1));
        }
        ws->phase = phase;
    }
    assert(alloc >= bottom);
    if (alloc < bottom) {
        ws->allocFailed = 1;
        return NULL;
    }
    ws->allocStart = alloc;
    return alloc;
}

/**
 * Unaligned.
 */
BYTE* ZSTD_cwksp_reserve_buffer(ZSTD_cwksp* ws, size_t bytes) {
    return (BYTE*)ZSTD_cwksp_reserve_internal(ws, bytes, ZSTD_cwksp_alloc_buffers);
}

/**
 * Aligned on sizeof(unsigned).
 */
void* ZSTD_cwksp_reserve_aligned(ZSTD_cwksp* ws, size_t bytes) {
    assert((bytes & (sizeof(U32)-1)) == 0); // TODO ???
    return ZSTD_cwksp_reserve_internal(ws, ZSTD_cwksp_align(bytes, sizeof(U32)), ZSTD_cwksp_alloc_aligned);
}

/**
 * Aligned on sizeof(unsigned). These buffers have the special property that
 * their values remain constrained, allowing us to re-use them without
 * memset()-ing them.
 */
void* ZSTD_cwksp_reserve_table(ZSTD_cwksp* ws, size_t bytes) {
    /* TODO(felixh): alignment */
    const ZSTD_cwksp_alloc_phase_e phase = ZSTD_cwksp_alloc_aligned;
    void* alloc = ws->tableEnd;
    void* end = (BYTE *)alloc + bytes;
    void* top = ws->allocStart;
    DEBUGLOG(3, "wksp: reserving table %zd bytes, %zd bytes remaining",
        bytes, (BYTE *)top - (BYTE *)end);
    assert((bytes & (sizeof(U32)-1)) == 0); // TODO ???
    assert(phase >= ws->phase);
    if (phase > ws->phase) {
        if (ws->phase <= ZSTD_cwksp_alloc_buffers) {

        }
        ws->phase = phase;
    }
    assert(end <= top);
    if (end > top) {
        ws->allocFailed = 1;
        return NULL;
    }
    ws->tableEnd = end;
    return alloc;
}

/**
 * Aligned on sizeof(void*).
 */
void* ZSTD_cwksp_reserve_object(ZSTD_cwksp* ws, size_t bytes) {
    size_t roundedBytes = ZSTD_cwksp_align(bytes, sizeof(void*));
    void* start = ws->objectEnd;
    void* end = (BYTE*)start + roundedBytes;
    DEBUGLOG(3, "wksp: reserving %zd bytes object (rounded to %zd), %zd bytes remaining", bytes, roundedBytes, (BYTE *)ws->workspaceEnd - (BYTE *)end);
    assert(((size_t)start & (sizeof(void*)-1)) == 0);
    assert((bytes & (sizeof(void*)-1)) == 0);
    if (ws->phase != ZSTD_cwksp_alloc_objects || end > ws->workspaceEnd) {
        DEBUGLOG(3, "wksp: object alloc failed!");
        ws->allocFailed = 1;
        return NULL;
    }
    ws->objectEnd = end;
    ws->tableEnd = end;
    return start;
}

/**
 * Invalidates table allocations.
 * All other allocations remain valid.
 */
void ZSTD_cwksp_clear_tables(ZSTD_cwksp* ws) {
    ws->tableEnd = ws->objectEnd;
}

/**
 * Invalidates all buffer, aligned, and table allocations.
 * Object allocations remain valid.
 */
void ZSTD_cwksp_clear(ZSTD_cwksp* ws) {
    DEBUGLOG(3, "wksp: clearing!");
    ws->tableEnd = ws->objectEnd;
    ws->allocStart = ws->workspaceEnd;
    ws->allocFailed = 0;
    if (ws->phase > ZSTD_cwksp_alloc_buffers) {
        ws->phase = ZSTD_cwksp_alloc_buffers;
    }
}

void ZSTD_cwksp_init(ZSTD_cwksp* ws, void* start, size_t size) {
    DEBUGLOG(3, "wksp: init'ing with %zd bytes", size);
    assert(((size_t)start & (sizeof(void*)-1)) == 0); /* ensure correct alignment */
    ws->workspace = start;
    ws->workspaceEnd = (BYTE*)start + size;
    ws->objectEnd = ws->workspace;
    ws->phase = ZSTD_cwksp_alloc_objects;
    ZSTD_cwksp_clear(ws);
    ws->workspaceOversizedDuration = 0;
}

size_t ZSTD_cwksp_create(ZSTD_cwksp* ws, size_t size, ZSTD_customMem customMem) {
    void* workspace = ZSTD_malloc(size, customMem);
    DEBUGLOG(3, "wksp: creating with %zd bytes", size);
    RETURN_ERROR_IF(workspace == NULL, memory_allocation);
    ZSTD_cwksp_init(ws, workspace, size);
    return 0;
}

void ZSTD_cwksp_free(ZSTD_cwksp* ws, ZSTD_customMem customMem) {
    DEBUGLOG(3, "wksp: freeing");
    ZSTD_free(ws->workspace, customMem);
    ws->workspace = NULL;
    ws->workspaceEnd = NULL;
    ZSTD_cwksp_clear(ws);
}

size_t ZSTD_cwksp_sizeof(const ZSTD_cwksp* ws) {
    return (BYTE*)ws->workspaceEnd - (BYTE*)ws->workspace;
}

int ZSTD_cwksp_reserve_failed(const ZSTD_cwksp* ws) {
    return ws->allocFailed;
}

size_t ZSTD_cwksp_available_space(ZSTD_cwksp* ws) {
    return (size_t)((BYTE*)ws->allocStart - (BYTE*)ws->tableEnd);
}

void ZSTD_cwksp_bump_oversized_duration(
        ZSTD_cwksp* ws, size_t additionalNeededSpace) {
    if (ZSTD_cwksp_check_too_large(ws, additionalNeededSpace)) {
        ws->workspaceOversizedDuration++;
    } else {
        ws->workspaceOversizedDuration = 0;
    }
}

int ZSTD_cwksp_check_available(ZSTD_cwksp* ws, size_t additionalNeededSpace) {
    return ZSTD_cwksp_available_space(ws) >= additionalNeededSpace;
}

int ZSTD_cwksp_check_too_large(ZSTD_cwksp* ws, size_t additionalNeededSpace) {
    return ZSTD_cwksp_check_available(
        ws, additionalNeededSpace * ZSTD_WORKSPACETOOLARGE_FACTOR);
}

int ZSTD_cwksp_check_wasteful(ZSTD_cwksp* ws, size_t additionalNeededSpace) {
    return ZSTD_cwksp_check_too_large(ws, additionalNeededSpace)
        && ws->workspaceOversizedDuration > ZSTD_WORKSPACETOOLARGE_MAXDURATION;
}

#if defined (__cplusplus)
}
#endif
