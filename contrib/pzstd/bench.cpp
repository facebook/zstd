/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#include "ErrorHolder.h"
#include "Options.h"
#include "Pzstd.h"
#include "utils/FileSystem.h"
#include "utils/Range.h"
#include "utils/ScopeGuard.h"
#include "utils/ThreadPool.h"
#include "utils/WorkQueue.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace pzstd;

namespace {
// Prints how many ns it was in scope for upon destruction
// Used for rough estimates of how long things took
struct BenchmarkTimer {
  using Clock = std::chrono::system_clock;
  Clock::time_point start;
  FILE* fd;

  explicit BenchmarkTimer(FILE* fd = stdout) : fd(fd) {
    start = Clock::now();
  }

  ~BenchmarkTimer() {
    auto end = Clock::now();
    size_t ticks =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    ticks = std::max(ticks, size_t{1});
    for (auto tmp = ticks; tmp < 100000; tmp *= 10) {
      std::fprintf(fd, " ");
    }
    std::fprintf(fd, "%zu | ", ticks);
  }
};
}

// Code I used for benchmarking

void testMain(const Options& options) {
  if (!options.decompress) {
    if (options.compressionLevel < 10) {
      std::printf("0");
    }
    std::printf("%u | ", options.compressionLevel);
  } else {
    std::printf(" d | ");
  }
  if (options.numThreads < 10) {
    std::printf("0");
  }
  std::printf("%u | ", options.numThreads);

  FILE* inputFd = std::fopen(options.inputFile.c_str(), "rb");
  if (inputFd == nullptr) {
    std::abort();
  }
  size_t inputSize = 0;
  if (inputFd != stdin) {
    std::error_code ec;
    inputSize = file_size(options.inputFile, ec);
    if (ec) {
      inputSize = 0;
    }
  }
  FILE* outputFd = std::fopen(options.outputFile.c_str(), "wb");
  if (outputFd == nullptr) {
    std::abort();
  }
  auto guard = makeScopeGuard([&] {
    std::fclose(inputFd);
    std::fclose(outputFd);
  });

  WorkQueue<std::shared_ptr<BufferWorkQueue>> outs;
  ErrorHolder errorHolder;
  size_t bytesWritten;
  {
    ThreadPool executor(options.numThreads);
    BenchmarkTimer timeIncludingClose;
    if (!options.decompress) {
      executor.add(
          [&errorHolder, &outs, &executor, inputFd, inputSize, &options] {
            asyncCompressChunks(
                errorHolder,
                outs,
                executor,
                inputFd,
                inputSize,
                options.numThreads,
                options.determineParameters());
          });
      bytesWritten = writeFile(errorHolder, outs, outputFd, true);
    } else {
      executor.add([&errorHolder, &outs, &executor, inputFd] {
        asyncDecompressFrames(errorHolder, outs, executor, inputFd);
      });
      bytesWritten = writeFile(
          errorHolder, outs, outputFd, /* writeSkippableFrames */ false);
    }
  }
  if (errorHolder.hasError()) {
    std::fprintf(stderr, "Error: %s.\n", errorHolder.getError().c_str());
    std::abort();
  }
  std::printf("%zu\n", bytesWritten);
}

int main(int argc, const char** argv) {
  if (argc < 3) {
    return 1;
  }
  Options options(0, 23, 0, false, "", "", true, true);
  // Benchmarking code
  for (size_t i = 0; i < 2; ++i) {
    for (size_t compressionLevel = 1; compressionLevel <= 16;
         compressionLevel <<= 1) {
      for (size_t numThreads = 1; numThreads <= 16; numThreads <<= 1) {
        options.numThreads = numThreads;
        options.compressionLevel = compressionLevel;
        options.decompress = false;
        options.inputFile = argv[1];
        options.outputFile = argv[2];
        testMain(options);
        options.decompress = true;
        options.inputFile = argv[2];
        options.outputFile = std::string(argv[1]) + ".d";
        testMain(options);
        std::fflush(stdout);
      }
    }
  }
  return 0;
}
