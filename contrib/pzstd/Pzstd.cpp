/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#include "Pzstd.h"
#include "SkippableFrame.h"
#include "utils/FileSystem.h"
#include "utils/Range.h"
#include "utils/ScopeGuard.h"
#include "utils/ThreadPool.h"
#include "utils/WorkQueue.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>    /* _O_BINARY */
#  include <io.h>       /* _setmode, _isatty */
#  define SET_BINARY_MODE(file) { if (_setmode(_fileno(file), _O_BINARY) == -1) perror("Cannot set _O_BINARY"); }
#else
#  include <unistd.h>   /* isatty */
#  define SET_BINARY_MODE(file)
#endif

namespace pzstd {

namespace {
#ifdef _WIN32
const std::string nullOutput = "nul";
#else
const std::string nullOutput = "/dev/null";
#endif
}

using std::size_t;

static std::uintmax_t fileSizeOrZero(const std::string &file) {
  if (file == "-") {
    return 0;
  }
  std::error_code ec;
  auto size = file_size(file, ec);
  if (ec) {
    size = 0;
  }
  return size;
}

static std::uint64_t handleOneInput(const Options &options,
                             const std::string &inputFile,
                             FILE* inputFd,
                             const std::string &outputFile,
                             FILE* outputFd,
                             ErrorHolder &errorHolder) {
  auto inputSize = fileSizeOrZero(inputFile);
  // WorkQueue outlives ThreadPool so in the case of error we are certain
  // we don't accidently try to call push() on it after it is destroyed.
  WorkQueue<std::shared_ptr<BufferWorkQueue>> outs{options.numThreads + 1};
  std::uint64_t bytesRead;
  std::uint64_t bytesWritten;
  {
    // Initialize the thread pool with numThreads + 1
    // We add one because the read thread spends most of its time waiting.
    // This also sets the minimum number of threads to 2, so the algorithm
    // doesn't deadlock.
    ThreadPool executor(options.numThreads + 1);
    if (!options.decompress) {
      // Add a job that reads the input and starts all the compression jobs
      executor.add(
          [&errorHolder, &outs, &executor, inputFd, inputSize, &options,
                                                               &bytesRead] {
            bytesRead = asyncCompressChunks(
                errorHolder,
                outs,
                executor,
                inputFd,
                inputSize,
                options.numThreads,
                options.determineParameters());
          });
      // Start writing
      bytesWritten = writeFile(errorHolder, outs, outputFd, options.decompress,
                               options.verbosity);
    } else {
      // Add a job that reads the input and starts all the decompression jobs
      executor.add([&errorHolder, &outs, &executor, inputFd, &bytesRead] {
        bytesRead = asyncDecompressFrames(errorHolder, outs, executor, inputFd);
      });
      // Start writing
      bytesWritten = writeFile(errorHolder, outs, outputFd, options.decompress,
                               options.verbosity);
    }
  }
  if (options.verbosity > 1 && !errorHolder.hasError()) {
    std::string inputFileName = inputFile == "-" ? "stdin" : inputFile;
    std::string outputFileName = outputFile == "-" ? "stdout" : outputFile;
    if (!options.decompress) {
      double ratio = static_cast<double>(bytesWritten) /
                     static_cast<double>(bytesRead + !bytesRead);
      std::fprintf(stderr, "%-20s :%6.2f%%   (%6llu => %6llu bytes, %s)\n",
                   inputFileName.c_str(), ratio * 100, bytesRead, bytesWritten,
                   outputFileName.c_str());
    } else {
      std::fprintf(stderr, "%-20s: %llu bytes \n",
                   inputFileName.c_str(),bytesWritten);
    }
  }
  return bytesWritten;
}

static FILE *openInputFile(const std::string &inputFile,
                           ErrorHolder &errorHolder) {
  if (inputFile == "-") {
    SET_BINARY_MODE(stdin);
    return stdin;
  }
  // Check if input file is a directory
  {
    std::error_code ec;
    if (is_directory(inputFile, ec)) {
      errorHolder.setError("Output file is a directory -- ignored");
      return nullptr;
    }
  }
  auto inputFd = std::fopen(inputFile.c_str(), "rb");
  if (!errorHolder.check(inputFd != nullptr, "Failed to open input file")) {
    return nullptr;
  }
  return inputFd;
}

static FILE *openOutputFile(const Options &options,
                            const std::string &outputFile,
                            ErrorHolder &errorHolder) {
  if (outputFile == "-") {
    SET_BINARY_MODE(stdout);
    return stdout;
  }
  // Check if the output file exists and then open it
  if (!options.overwrite && outputFile != nullOutput) {
    auto outputFd = std::fopen(outputFile.c_str(), "rb");
    if (outputFd != nullptr) {
      std::fclose(outputFd);
      if (options.verbosity <= 1) {
        errorHolder.setError("Output file exists");
        return nullptr;
      }
      std::fprintf(
          stderr,
          "pzstd: %s already exists; do you wish to overwrite (y/n) ? ",
          outputFile.c_str());
      int c = getchar();
      if (c != 'y' && c != 'Y') {
        errorHolder.setError("Not overwritten");
        return nullptr;
      }
    }
  }
  auto outputFd = std::fopen(outputFile.c_str(), "wb");
  if (!errorHolder.check(
          outputFd != nullptr, "Failed to open output file")) {
    return 0;
  }
  return outputFd;
}

int pzstdMain(const Options &options) {
  int returnCode = 0;
  for (const auto& input : options.inputFiles) {
    // Setup the error holder
    ErrorHolder errorHolder;
    auto printErrorGuard = makeScopeGuard([&] {
      if (errorHolder.hasError()) {
        returnCode = 1;
        if (options.verbosity > 0) {
          std::fprintf(stderr, "pzstd: %s: %s.\n", input.c_str(),
                       errorHolder.getError().c_str());
        }
      } else {

      }
    });
    // Open the input file
    auto inputFd = openInputFile(input, errorHolder);
    if (inputFd == nullptr) {
      continue;
    }
    auto closeInputGuard = makeScopeGuard([&] { std::fclose(inputFd); });
    // Open the output file
    auto outputFile = options.getOutputFile(input);
    if (!errorHolder.check(outputFile != "",
                           "Input file does not have extension .zst")) {
      continue;
    }
    auto outputFd = openOutputFile(options, outputFile, errorHolder);
    if (outputFd == nullptr) {
      continue;
    }
    auto closeOutputGuard = makeScopeGuard([&] { std::fclose(outputFd); });
    // (de)compress the file
    handleOneInput(options, input, inputFd, outputFile, outputFd, errorHolder);
    if (errorHolder.hasError()) {
      continue;
    }
    // Delete the input file if necessary
    if (!options.keepSource) {
      // Be sure that we are done and have written everything before we delete
      if (!errorHolder.check(std::fclose(inputFd) == 0,
                             "Failed to close input file")) {
        continue;
      }
      closeInputGuard.dismiss();
      if (!errorHolder.check(std::fclose(outputFd) == 0,
                             "Failed to close output file")) {
        continue;
      }
      closeOutputGuard.dismiss();
      if (std::remove(input.c_str()) != 0) {
        errorHolder.setError("Failed to remove input file");
        continue;
      }
    }
  }
  // Returns 1 if any of the files failed to (de)compress.
  return returnCode;
}

/// Construct a `ZSTD_inBuffer` that points to the data in `buffer`.
static ZSTD_inBuffer makeZstdInBuffer(const Buffer& buffer) {
  return ZSTD_inBuffer{buffer.data(), buffer.size(), 0};
}

/**
 * Advance `buffer` and `inBuffer` by the amount of data read, as indicated by
 * `inBuffer.pos`.
 */
void advance(Buffer& buffer, ZSTD_inBuffer& inBuffer) {
  auto pos = inBuffer.pos;
  inBuffer.src = static_cast<const unsigned char*>(inBuffer.src) + pos;
  inBuffer.size -= pos;
  inBuffer.pos = 0;
  return buffer.advance(pos);
}

/// Construct a `ZSTD_outBuffer` that points to the data in `buffer`.
static ZSTD_outBuffer makeZstdOutBuffer(Buffer& buffer) {
  return ZSTD_outBuffer{buffer.data(), buffer.size(), 0};
}

/**
 * Split `buffer` and advance `outBuffer` by the amount of data written, as
 * indicated by `outBuffer.pos`.
 */
Buffer split(Buffer& buffer, ZSTD_outBuffer& outBuffer) {
  auto pos = outBuffer.pos;
  outBuffer.dst = static_cast<unsigned char*>(outBuffer.dst) + pos;
  outBuffer.size -= pos;
  outBuffer.pos = 0;
  return buffer.splitAt(pos);
}

/**
 * Stream chunks of input from `in`, compress it, and stream it out to `out`.
 *
 * @param errorHolder Used to report errors and check if an error occured
 * @param in           Queue that we `pop()` input buffers from
 * @param out          Queue that we `push()` compressed output buffers to
 * @param maxInputSize An upper bound on the size of the input
 * @param parameters   The zstd parameters to use for compression
 */
static void compress(
    ErrorHolder& errorHolder,
    std::shared_ptr<BufferWorkQueue> in,
    std::shared_ptr<BufferWorkQueue> out,
    size_t maxInputSize,
    ZSTD_parameters parameters) {
  auto guard = makeScopeGuard([&] { out->finish(); });
  // Initialize the CCtx
  std::unique_ptr<ZSTD_CStream, size_t (*)(ZSTD_CStream*)> ctx(
      ZSTD_createCStream(), ZSTD_freeCStream);
  if (!errorHolder.check(ctx != nullptr, "Failed to allocate ZSTD_CStream")) {
    return;
  }
  {
    auto err = ZSTD_initCStream_advanced(ctx.get(), nullptr, 0, parameters, 0);
    if (!errorHolder.check(!ZSTD_isError(err), ZSTD_getErrorName(err))) {
      return;
    }
  }

  // Allocate space for the result
  auto outBuffer = Buffer(ZSTD_compressBound(maxInputSize));
  auto zstdOutBuffer = makeZstdOutBuffer(outBuffer);
  {
    Buffer inBuffer;
    // Read a buffer in from the input queue
    while (in->pop(inBuffer) && !errorHolder.hasError()) {
      auto zstdInBuffer = makeZstdInBuffer(inBuffer);
      // Compress the whole buffer and send it to the output queue
      while (!inBuffer.empty() && !errorHolder.hasError()) {
        if (!errorHolder.check(
                !outBuffer.empty(), "ZSTD_compressBound() was too small")) {
          return;
        }
        // Compress
        auto err =
            ZSTD_compressStream(ctx.get(), &zstdOutBuffer, &zstdInBuffer);
        if (!errorHolder.check(!ZSTD_isError(err), ZSTD_getErrorName(err))) {
          return;
        }
        // Split the compressed data off outBuffer and pass to the output queue
        out->push(split(outBuffer, zstdOutBuffer));
        // Forget about the data we already compressed
        advance(inBuffer, zstdInBuffer);
      }
    }
  }
  // Write the epilog
  size_t bytesLeft;
  do {
    if (!errorHolder.check(
            !outBuffer.empty(), "ZSTD_compressBound() was too small")) {
      return;
    }
    bytesLeft = ZSTD_endStream(ctx.get(), &zstdOutBuffer);
    if (!errorHolder.check(
            !ZSTD_isError(bytesLeft), ZSTD_getErrorName(bytesLeft))) {
      return;
    }
    out->push(split(outBuffer, zstdOutBuffer));
  } while (bytesLeft != 0 && !errorHolder.hasError());
}

/**
 * Calculates how large each independently compressed frame should be.
 *
 * @param size       The size of the source if known, 0 otherwise
 * @param numThreads The number of threads available to run compression jobs on
 * @param params     The zstd parameters to be used for compression
 */
static size_t calculateStep(
    std::uintmax_t size,
    size_t numThreads,
    const ZSTD_parameters &params) {
  size_t step = size_t{1} << (params.cParams.windowLog + 2);
  // If file size is known, see if a smaller step will spread work more evenly
  if (size != 0) {
    const std::uintmax_t newStep = size / numThreads;
    if (newStep != 0 && newStep <= std::numeric_limits<size_t>::max()) {
      step = std::min(step, static_cast<size_t>(newStep));
    }
  }
  return step;
}

namespace {
enum class FileStatus { Continue, Done, Error };
/// Determines the status of the file descriptor `fd`.
FileStatus fileStatus(FILE* fd) {
  if (std::feof(fd)) {
    return FileStatus::Done;
  } else if (std::ferror(fd)) {
    return FileStatus::Error;
  }
  return FileStatus::Continue;
}
} // anonymous namespace

/**
 * Reads `size` data in chunks of `chunkSize` and puts it into `queue`.
 * Will read less if an error or EOF occurs.
 * Returns the status of the file after all of the reads have occurred.
 */
static FileStatus
readData(BufferWorkQueue& queue, size_t chunkSize, size_t size, FILE* fd,
         std::uint64_t *totalBytesRead) {
  Buffer buffer(size);
  while (!buffer.empty()) {
    auto bytesRead =
        std::fread(buffer.data(), 1, std::min(chunkSize, buffer.size()), fd);
    *totalBytesRead += bytesRead;
    queue.push(buffer.splitAt(bytesRead));
    auto status = fileStatus(fd);
    if (status != FileStatus::Continue) {
      return status;
    }
  }
  return FileStatus::Continue;
}

std::uint64_t asyncCompressChunks(
    ErrorHolder& errorHolder,
    WorkQueue<std::shared_ptr<BufferWorkQueue>>& chunks,
    ThreadPool& executor,
    FILE* fd,
    std::uintmax_t size,
    size_t numThreads,
    ZSTD_parameters params) {
  auto chunksGuard = makeScopeGuard([&] { chunks.finish(); });
  std::uint64_t bytesRead = 0;

  // Break the input up into chunks of size `step` and compress each chunk
  // independently.
  size_t step = calculateStep(size, numThreads, params);
  auto status = FileStatus::Continue;
  while (status == FileStatus::Continue && !errorHolder.hasError()) {
    // Make a new input queue that we will put the chunk's input data into.
    auto in = std::make_shared<BufferWorkQueue>();
    auto inGuard = makeScopeGuard([&] { in->finish(); });
    // Make a new output queue that compress will put the compressed data into.
    auto out = std::make_shared<BufferWorkQueue>();
    // Start compression in the thread pool
    executor.add([&errorHolder, in, out, step, params] {
      return compress(
          errorHolder, std::move(in), std::move(out), step, params);
    });
    // Pass the output queue to the writer thread.
    chunks.push(std::move(out));
    // Fill the input queue for the compression job we just started
    status = readData(*in, ZSTD_CStreamInSize(), step, fd, &bytesRead);
  }
  errorHolder.check(status != FileStatus::Error, "Error reading input");
  return bytesRead;
}

/**
 * Decompress a frame, whose data is streamed into `in`, and stream the output
 * to `out`.
 *
 * @param errorHolder Used to report errors and check if an error occured
 * @param in           Queue that we `pop()` input buffers from. It contains
 *                      exactly one compressed frame.
 * @param out          Queue that we `push()` decompressed output buffers to
 */
static void decompress(
    ErrorHolder& errorHolder,
    std::shared_ptr<BufferWorkQueue> in,
    std::shared_ptr<BufferWorkQueue> out) {
  auto guard = makeScopeGuard([&] { out->finish(); });
  // Initialize the DCtx
  std::unique_ptr<ZSTD_DStream, size_t (*)(ZSTD_DStream*)> ctx(
      ZSTD_createDStream(), ZSTD_freeDStream);
  if (!errorHolder.check(ctx != nullptr, "Failed to allocate ZSTD_DStream")) {
    return;
  }
  {
    auto err = ZSTD_initDStream(ctx.get());
    if (!errorHolder.check(!ZSTD_isError(err), ZSTD_getErrorName(err))) {
      return;
    }
  }

  const size_t outSize = ZSTD_DStreamOutSize();
  Buffer inBuffer;
  size_t returnCode = 0;
  // Read a buffer in from the input queue
  while (in->pop(inBuffer) && !errorHolder.hasError()) {
    auto zstdInBuffer = makeZstdInBuffer(inBuffer);
    // Decompress the whole buffer and send it to the output queue
    while (!inBuffer.empty() && !errorHolder.hasError()) {
      // Allocate a buffer with at least outSize bytes.
      Buffer outBuffer(outSize);
      auto zstdOutBuffer = makeZstdOutBuffer(outBuffer);
      // Decompress
      returnCode =
          ZSTD_decompressStream(ctx.get(), &zstdOutBuffer, &zstdInBuffer);
      if (!errorHolder.check(
              !ZSTD_isError(returnCode), ZSTD_getErrorName(returnCode))) {
        return;
      }
      // Pass the buffer with the decompressed data to the output queue
      out->push(split(outBuffer, zstdOutBuffer));
      // Advance past the input we already read
      advance(inBuffer, zstdInBuffer);
      if (returnCode == 0) {
        // The frame is over, prepare to (maybe) start a new frame
        ZSTD_initDStream(ctx.get());
      }
    }
  }
  if (!errorHolder.check(returnCode <= 1, "Incomplete block")) {
    return;
  }
  // We've given ZSTD_decompressStream all of our data, but there may still
  // be data to read.
  while (returnCode == 1) {
    // Allocate a buffer with at least outSize bytes.
    Buffer outBuffer(outSize);
    auto zstdOutBuffer = makeZstdOutBuffer(outBuffer);
    // Pass in no input.
    ZSTD_inBuffer zstdInBuffer{nullptr, 0, 0};
    // Decompress
    returnCode =
        ZSTD_decompressStream(ctx.get(), &zstdOutBuffer, &zstdInBuffer);
    if (!errorHolder.check(
            !ZSTD_isError(returnCode), ZSTD_getErrorName(returnCode))) {
      return;
    }
    // Pass the buffer with the decompressed data to the output queue
    out->push(split(outBuffer, zstdOutBuffer));
  }
}

std::uint64_t asyncDecompressFrames(
    ErrorHolder& errorHolder,
    WorkQueue<std::shared_ptr<BufferWorkQueue>>& frames,
    ThreadPool& executor,
    FILE* fd) {
  auto framesGuard = makeScopeGuard([&] { frames.finish(); });
  std::uint64_t totalBytesRead = 0;

  // Split the source up into its component frames.
  // If we find our recognized skippable frame we know the next frames size
  // which means that we can decompress each standard frame in independently.
  // Otherwise, we will decompress using only one decompression task.
  const size_t chunkSize = ZSTD_DStreamInSize();
  auto status = FileStatus::Continue;
  while (status == FileStatus::Continue && !errorHolder.hasError()) {
    // Make a new input queue that we will put the frames's bytes into.
    auto in = std::make_shared<BufferWorkQueue>();
    auto inGuard = makeScopeGuard([&] { in->finish(); });
    // Make a output queue that decompress will put the decompressed data into
    auto out = std::make_shared<BufferWorkQueue>();

    size_t frameSize;
    {
      // Calculate the size of the next frame.
      // frameSize is 0 if the frame info can't be decoded.
      Buffer buffer(SkippableFrame::kSize);
      auto bytesRead = std::fread(buffer.data(), 1, buffer.size(), fd);
      totalBytesRead += bytesRead;
      status = fileStatus(fd);
      if (bytesRead == 0 && status != FileStatus::Continue) {
        break;
      }
      buffer.subtract(buffer.size() - bytesRead);
      frameSize = SkippableFrame::tryRead(buffer.range());
      in->push(std::move(buffer));
    }
    if (frameSize == 0) {
      // We hit a non SkippableFrame, so this will be the last job.
      // Make sure that we don't use too much memory
      in->setMaxSize(64);
      out->setMaxSize(64);
    }
    // Start decompression in the thread pool
    executor.add([&errorHolder, in, out] {
      return decompress(errorHolder, std::move(in), std::move(out));
    });
    // Pass the output queue to the writer thread
    frames.push(std::move(out));
    if (frameSize == 0) {
      // We hit a non SkippableFrame ==> not compressed by pzstd or corrupted
      // Pass the rest of the source to this decompression task
      while (status == FileStatus::Continue && !errorHolder.hasError()) {
        status = readData(*in, chunkSize, chunkSize, fd, &totalBytesRead);
      }
      break;
    }
    // Fill the input queue for the decompression job we just started
    status = readData(*in, chunkSize, frameSize, fd, &totalBytesRead);
  }
  errorHolder.check(status != FileStatus::Error, "Error reading input");
  return totalBytesRead;
}

/// Write `data` to `fd`, returns true iff success.
static bool writeData(ByteRange data, FILE* fd) {
  while (!data.empty()) {
    data.advance(std::fwrite(data.begin(), 1, data.size(), fd));
    if (std::ferror(fd)) {
      return false;
    }
  }
  return true;
}

void updateWritten(int verbosity, std::uint64_t bytesWritten) {
  if (verbosity <= 1) {
    return;
  }
  using Clock = std::chrono::system_clock;
  static Clock::time_point then;
  constexpr std::chrono::milliseconds refreshRate{150};

  auto now = Clock::now();
  if (now - then > refreshRate) {
    then = now;
    std::fprintf(stderr, "\rWritten: %u MB   ",
                 static_cast<std::uint32_t>(bytesWritten >> 20));
  }
}

std::uint64_t writeFile(
    ErrorHolder& errorHolder,
    WorkQueue<std::shared_ptr<BufferWorkQueue>>& outs,
    FILE* outputFd,
    bool decompress,
    int verbosity) {
  auto lineClearGuard = makeScopeGuard([verbosity] {
    if (verbosity > 1) {
      std::fprintf(stderr, "\r%79s\r", "");
    }
  });
  std::uint64_t bytesWritten = 0;
  std::shared_ptr<BufferWorkQueue> out;
  // Grab the output queue for each decompression job (in order).
  while (outs.pop(out) && !errorHolder.hasError()) {
    if (!decompress) {
      // If we are compressing and want to write skippable frames we can't
      // start writing before compression is done because we need to know the
      // compressed size.
      // Wait for the compressed size to be available and write skippable frame
      SkippableFrame frame(out->size());
      if (!writeData(frame.data(), outputFd)) {
        errorHolder.setError("Failed to write output");
        return bytesWritten;
      }
      bytesWritten += frame.kSize;
    }
    // For each chunk of the frame: Pop it from the queue and write it
    Buffer buffer;
    while (out->pop(buffer) && !errorHolder.hasError()) {
      if (!writeData(buffer.range(), outputFd)) {
        errorHolder.setError("Failed to write output");
        return bytesWritten;
      }
      bytesWritten += buffer.size();
      updateWritten(verbosity, bytesWritten);
    }
  }
  return bytesWritten;
}
}
