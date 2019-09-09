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
 * from this workspace for each internal datastructure:
 *
 * - These different internal datastructures have different setup requirements:
 *
 *   - The static objects need to be cleared once and can then be trivially
 *     reused for each compression.
 *
 *   - Various buffers don't need to be initialized at all--they are always
 *     written into before they're read.
 *
 *   - The matchstate tables have a unique requirement that they don't need
 *     their memory to be totally cleared, but they do need the memory to have
 *     some bound, i.e., a guarantee that all values in the memory they've been
 *     allocated is less than some maximum value (which is the starting value
 *     for the indices that they will then use for compression). When this
 *     guarantee is provided to them, they can use the memory without any setup
 *     work. When it can't, they have to clear the area.
 *
 * - These buffers also have different alignment requirements.
 *
 * - We would like to reuse the objects in the workspace for multiple
 *   compressions without having to perform any expensive reallocation or
 *   reinitialization work.
 *
 * - We would like to be able to efficiently reuse the workspace across
 *   multiple compressions **even when the compression parameters change** and
 *   we need to resize some of the objects (where possible).
 *
 * To attempt to manage this buffer, given these constraints, the ZSTD_cwksp
 * abstraction was created. It works as follows:
 *
 * Workspace Layout:
 *
 * [                        ... workspace ...                         ]
 * [objects][tables ... ->] free space [<- ... aligned][<- ... buffers]
 *
 * The various objects that live in the workspace are divided into the
 * following categories, and are allocated separately:
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
 * - Aligned: these buffers are used for various purposes that require 4 byte
 *   alignment, but don't require any initialization before they're used.
 *
 * - Buffers: these buffers are used for various purposes that don't require
 *   any alignment or initialization before they're used. This means they can
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
 * Attempts to reserve objects of different types out of order will fail.
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

/**
 * The provided workspace takes ownership of the buffer [start, start+size).
 * Any existing values in the workspace are ignored (the previously managed
 * buffer, if present, must be separately freed).
 */
void ZSTD_cwksp_init(ZSTD_cwksp* ws, void* start, size_t size);

size_t ZSTD_cwksp_create(ZSTD_cwksp* ws, size_t size, ZSTD_customMem customMem);

void ZSTD_cwksp_free(ZSTD_cwksp* ws, ZSTD_customMem customMem);

/**
 * Moves the management of a workspace from one cwksp to another. The src cwksp
 * is left in an invalid state (src must be re-init()'ed before its used again).
 */
void ZSTD_cwksp_move(ZSTD_cwksp* dst, ZSTD_cwksp* src);

size_t ZSTD_cwksp_sizeof(const ZSTD_cwksp* ws);

int ZSTD_cwksp_reserve_failed(const ZSTD_cwksp* ws);

/*-*************************************
*  Functions Checking Free Space
***************************************/

size_t ZSTD_cwksp_available_space(ZSTD_cwksp* ws);

int ZSTD_cwksp_check_available(ZSTD_cwksp* ws, size_t additionalNeededSpace);

int ZSTD_cwksp_check_too_large(ZSTD_cwksp* ws, size_t additionalNeededSpace);

int ZSTD_cwksp_check_wasteful(ZSTD_cwksp* ws, size_t additionalNeededSpace);

void ZSTD_cwksp_bump_oversized_duration(ZSTD_cwksp* ws, size_t additionalNeededSpace);

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_CWKSP_H */
