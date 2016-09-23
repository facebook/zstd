/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#pragma once

#include "ErrorHolder.h"
#include "Options.h"
#include "utils/Buffer.h"
#include "utils/Range.h"
#include "utils/ThreadPool.h"
#include "utils/WorkQueue.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#undef ZSTD_STATIC_LINKING_ONLY

#include <cstddef>
#include <cstdint>
#include <memory>

namespace pzstd {
/**
 * Runs pzstd with `options` and returns the number of bytes written.
 * An error occurred if `errorHandler.hasError()`.
 *
 * @param options      The pzstd options to use for (de)compression
 * @returns            0 upon success and non-zero on failure.
 */
int pzstdMain(const Options& options);

/**
 * Streams input from `fd`, breaks input up into chunks, and compresses each
 * chunk independently.  Output of each chunk gets streamed to a queue, and
 * the output queues get put into `chunks` in order.
 *
 * @param errorHolder  Used to report errors and coordinate early shutdown
 * @param chunks       Each compression jobs output queue gets `pushed()` here
 *                      as soon as it is available
 * @param executor     The thread pool to run compression jobs in
 * @param fd           The input file descriptor
 * @param size         The size of the input file if known, 0 otherwise
 * @param numThreads   The number of threads in the thread pool
 * @param parameters   The zstd parameters to use for compression
 * @returns            The number of bytes read from the file
 */
std::uint64_t asyncCompressChunks(
    ErrorHolder& errorHolder,
    WorkQueue<std::shared_ptr<BufferWorkQueue>>& chunks,
    ThreadPool& executor,
    FILE* fd,
    std::uintmax_t size,
    std::size_t numThreads,
    ZSTD_parameters parameters);

/**
 * Streams input from `fd`.  If pzstd headers are available it breaks the input
 * up into independent frames.  It sends each frame to an independent
 * decompression job.  Output of each frame gets streamed to a queue, and
 * the output queues get put into `frames` in order.
 *
 * @param errorHolder  Used to report errors and coordinate early shutdown
 * @param frames       Each decompression jobs output queue gets `pushed()` here
 *                      as soon as it is available
 * @param executor     The thread pool to run compression jobs in
 * @param fd           The input file descriptor
 * @returns            The number of bytes read from the file
 */
std::uint64_t asyncDecompressFrames(
    ErrorHolder& errorHolder,
    WorkQueue<std::shared_ptr<BufferWorkQueue>>& frames,
    ThreadPool& executor,
    FILE* fd);

/**
 * Streams input in from each queue in `outs` in order, and writes the data to
 * `outputFd`.
 *
 * @param errorHolder  Used to report errors and coordinate early exit
 * @param outs         A queue of output queues, one for each
 *                      (de)compression job.
 * @param outputFd     The file descriptor to write to
 * @param decompress   Are we decompressing?
 * @param verbosity    The verbosity level to log at
 * @returns            The number of bytes written
 */
std::uint64_t writeFile(
    ErrorHolder& errorHolder,
    WorkQueue<std::shared_ptr<BufferWorkQueue>>& outs,
    FILE* outputFd,
    bool decompress,
    int verbosity);
}
