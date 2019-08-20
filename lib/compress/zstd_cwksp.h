/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_CWKSP_H
#define ZSTD_CWKSP_H

/*-*************************************
*  Dependencies
***************************************/
#include "zstd_internal.h"

#if defined (__cplusplus)
extern "C" {
#endif

/*-*************************************
*  Constants
***************************************/

/* define "workspace is too large" as this number of times larger than needed */
#define ZSTD_WORKSPACETOOLARGE_FACTOR 3

/* when workspace is continuously too large
 * during at least this number of times,
 * context's memory usage is considered wasteful,
 * because it's sized to handle a worst case scenario which rarely happens.
 * In which case, resize it down to free some memory */
#define ZSTD_WORKSPACETOOLARGE_MAXDURATION 128

/*-*************************************
*  Structures
***************************************/
typedef enum {
    ZSTD_cwksp_alloc_objects,
    ZSTD_cwksp_alloc_buffers,
    ZSTD_cwksp_alloc_aligned
} ZSTD_cwksp_alloc_phase_e;

/**
 * Zstd fits all its internal datastructures into a single continuous buffer,
 * so that it only needs to perform a single OS allocation (or so that a buffer
 * can be provided to it and it can perform no allocations at all). This buffer
 * is called the workspace.
 *
 * Several optimizations complicate that process of allocating memory ranges
 * from this workspace for each datastructure:
 *
 * - These different internal datastructures have different setup requirements.
 *   Some (e.g., the window buffer) don't care, and are happy to accept
 *   uninitialized memory. Others (e.g., the matchstate tables) can accept
 *   memory filled with unknown but bounded values (i.e., a memory area whose 
 *   values are known to be constrained between 0 and some upper bound). If
 *   that constraint isn't known to be satisfied, the area has to be cleared.
 *
 * - We would like to reuse the objects in the workspace for multiple
 *   compressions without having to perform any expensive reallocation or
 *   reinitialization work.
 *
 * - We would like to be able to efficiently reuse the workspace across
 *   multiple compressions **even when the compression parameters change** and
 *   we need to resize some of the objects (where possible).
 *
 * Workspace Layout:
 *
 * [                        ... workspace ...                         ]
 * [objects][tables ... ->] free space [<- ... aligned][<- ... buffers]
 *
 * In order to accomplish this, the various objects that live in the workspace
 * are divided into the following categories:
 *
 * - Static objects: this is optionally the enclosing ZSTD_CCtx or ZSTD_CDict,
 *   so that literally everything fits in a single buffer. Note: if present,
 *   this must be the first object in the workspace, since ZSTD_free{CCtx,
 *   CDict}() rely on a pointer comparison to see whether one or two frees are
 *   required.
 *
 * - Fixed size objects: these are fixed-size, fixed-count objects that are
 *   nonetheless "dynamically" allocated in the workspace so that we can
 *   control how they're initialized separately from the broader ZSTD_CCtx.
 *   Examples:
 *   - Entropy Workspace
 *   - 2 x ZSTD_compressedBlockState_t
 *   - CDict dictionary contents
 *
 * - Tables: these are any of several different datastructures (hash tables,
 *   chain tables, binary trees) that all respect a common format: they are
 *   uint32_t arrays, all of whose values are between 0 and (nextSrc - base).
 *   Their sizes depend on the cparams.
 *
 * - Aligned: these buffers are used for various purposes that don't require
 *   any initialization before they're used.
 *
 * - Uninitialized memory: these buffers are used for various purposes that
 *   don't require any initialization before they're used. This means they can
 *   be moved around at no cost for a new compression.
 *
 * Allocating Memory:
 *
 * The various types of objects must be allocated in order, so they can be
 * correctly packed into the workspace buffer. That order is:
 *
 * 1. Objects
 * 2. Buffers
 * 3. Aligned
 * 4. Tables
 *
 * Reusing Table Space:
 *
 * TODO(felixh): ...
 */
typedef struct {
    void* workspace;
    void* workspaceEnd;

    void* objectEnd;
    void* tableEnd;
    void* allocStart;

    int allocFailed;
    int workspaceOversizedDuration;
    ZSTD_cwksp_alloc_phase_e phase;
} ZSTD_cwksp;

/*-*************************************
*  Functions
***************************************/

/**
 * Align must be a power of 2.
 */
size_t ZSTD_cwksp_align(size_t size, size_t const align);

/**
 * Unaligned.
 */
BYTE* ZSTD_cwksp_reserve_buffer(ZSTD_cwksp* ws, size_t bytes);

/**
 * Aligned on sizeof(unsigned).
 */
void* ZSTD_cwksp_reserve_aligned(ZSTD_cwksp* ws, size_t bytes);

/**
 * Aligned on sizeof(unsigned). These buffers have the special property that
 * their values remain constrained, allowing us to re-use them without
 * memset()-ing them.
 */
void* ZSTD_cwksp_reserve_table(ZSTD_cwksp* ws, size_t bytes);

/**
 * Aligned on sizeof(void*).
 */
void* ZSTD_cwksp_reserve_object(ZSTD_cwksp* ws, size_t bytes);

int ZSTD_cwksp_bump_oversized_duration(ZSTD_cwksp* ws);

/**
 * Invalidates table allocations.
 * All other allocations remain valid.
 */
void ZSTD_cwksp_clear_tables(ZSTD_cwksp* ws);

/**
 * Invalidates all buffer, aligned, and table allocations.
 * Object allocations remain valid.
 */
void ZSTD_cwksp_clear(ZSTD_cwksp* ws);

void ZSTD_cwksp_init(ZSTD_cwksp* ws, void* start, size_t size);

size_t ZSTD_cwksp_create(ZSTD_cwksp* ws, size_t size, ZSTD_customMem customMem);

void ZSTD_cwksp_free(ZSTD_cwksp* ws, ZSTD_customMem customMem);

size_t ZSTD_cwksp_available_space(ZSTD_cwksp* ws);

int ZSTD_cwksp_check_available(ZSTD_cwksp* ws, size_t minFree);

int ZSTD_cwksp_check_wasteful(ZSTD_cwksp* ws, size_t minFree);

size_t ZSTD_cwksp_sizeof(const ZSTD_cwksp* ws);

int ZSTD_cwksp_reserve_failed(const ZSTD_cwksp* ws);

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_CWKSP_H */
