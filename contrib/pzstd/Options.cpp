/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#include "Options.h"

#include <cstdio>
#include <cstring>
#include <thread>

namespace pzstd {

namespace {
unsigned parseUnsigned(const char* arg) {
  unsigned result = 0;
  while (*arg >= '0' && *arg <= '9') {
    result *= 10;
    result += *arg - '0';
    ++arg;
  }
  return result;
}

const std::string zstdExtension = ".zst";
constexpr unsigned defaultCompressionLevel = 3;
constexpr unsigned maxNonUltraCompressionLevel = 19;

void usage() {
  std::fprintf(stderr, "Usage:\n");
  std::fprintf(stderr, "\tpzstd [args] FILE\n");
  std::fprintf(stderr, "Parallel ZSTD options:\n");
  std::fprintf(stderr, "\t-n/--num-threads #: Number of threads to spawn\n");
  std::fprintf(stderr, "\t-p/--pzstd-headers: Write pzstd headers to enable parallel decompression\n");

  std::fprintf(stderr, "ZSTD options:\n");
  std::fprintf(stderr, "\t-u/--ultra        : enable levels beyond %i, up to %i (requires more memory)\n", maxNonUltraCompressionLevel, ZSTD_maxCLevel());
  std::fprintf(stderr, "\t-h/--help         : display help and exit\n");
  std::fprintf(stderr, "\t-V/--version      : display version number and exit\n");
  std::fprintf(stderr, "\t-d/--decompress   : decompression\n");
  std::fprintf(stderr, "\t-f/--force        : overwrite output\n");
  std::fprintf(stderr, "\t-o/--output file  : result stored into `file`\n");
  std::fprintf(stderr, "\t-c/--stdout       : write output to standard output\n");
  std::fprintf(stderr, "\t-#                : # compression level (1-%d, default:%d)\n", maxNonUltraCompressionLevel, defaultCompressionLevel);
}
} // anonymous namespace

Options::Options()
    : numThreads(0),
      maxWindowLog(23),
      compressionLevel(defaultCompressionLevel),
      decompress(false),
      overwrite(false),
      pzstdHeaders(false) {}

bool Options::parse(int argc, const char** argv) {
  bool ultra = false;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    // Arguments with a short option
    char option = 0;
    if (!std::strcmp(arg, "--num-threads")) {
      option = 'n';
    } else if (!std::strcmp(arg, "--pzstd-headers")) {
      option = 'p';
    } else if (!std::strcmp(arg, "--ultra")) {
      option = 'u';
    } else if (!std::strcmp(arg, "--version")) {
      option = 'V';
    } else if (!std::strcmp(arg, "--help")) {
      option = 'h';
    } else if (!std::strcmp(arg, "--decompress")) {
      option = 'd';
    } else if (!std::strcmp(arg, "--force")) {
      option = 'f';
    } else if (!std::strcmp(arg, "--output")) {
      option = 'o';
    }  else if (!std::strcmp(arg, "--stdout")) {
      option = 'c';
    }else if (arg[0] == '-' && arg[1] != 0) {
      // Parse the compression level or short option
      if (arg[1] >= '0' && arg[1] <= '9') {
        compressionLevel = parseUnsigned(arg + 1);
        continue;
      }
      option = arg[1];
    } else if (inputFile.empty()) {
      inputFile = arg;
      continue;
    } else {
      std::fprintf(stderr, "Invalid argument: %s.\n", arg);
      return false;
    }

    switch (option) {
      case 'n':
        if (++i == argc) {
          std::fprintf(stderr, "Invalid argument: -n requires an argument.\n");
          return false;
        }
        numThreads = parseUnsigned(argv[i]);
        if (numThreads == 0) {
          std::fprintf(stderr, "Invalid argument: # of threads must be > 0.\n");
          return false;
        }
        break;
      case 'p':
        pzstdHeaders = true;
        break;
      case 'u':
        ultra = true;
        maxWindowLog = 0;
        break;
      case 'V':
        std::fprintf(stderr, "ZSTD version: %s.\n", ZSTD_VERSION_STRING);
        return false;
      case 'h':
        usage();
        return false;
      case 'd':
        decompress = true;
        break;
      case 'f':
        overwrite = true;
        break;
      case 'o':
        if (++i == argc) {
          std::fprintf(stderr, "Invalid argument: -o requires an argument.\n");
          return false;
        }
        outputFile = argv[i];
        break;
      case 'c':
        outputFile = '-';
        break;
      default:
        std::fprintf(stderr, "Invalid argument: %s.\n", arg);
        return false;
    }
  }
  // Determine input file if not specified
  if (inputFile.empty()) {
    inputFile = "-";
  }
  // Determine output file if not specified
  if (outputFile.empty()) {
    if (inputFile == "-") {
      outputFile = "-";
    } else {
      // Attempt to add/remove zstd extension from the input file
      if (decompress) {
        int stemSize = inputFile.size() - zstdExtension.size();
        if (stemSize > 0 && inputFile.substr(stemSize) == zstdExtension) {
          outputFile = inputFile.substr(0, stemSize);
        } else {
          std::fprintf(
              stderr, "Invalid argument: Unable to determine output file.\n");
          return false;
        }
      } else {
        outputFile = inputFile + zstdExtension;
      }
    }
  }
  // Check compression level
  {
    unsigned maxCLevel = ultra ? ZSTD_maxCLevel() : maxNonUltraCompressionLevel;
    if (compressionLevel > maxCLevel) {
      std::fprintf(
          stderr, "Invalid compression level %u.\n", compressionLevel);
      return false;
    }
  }
  // Check that numThreads is set
  if (numThreads == 0) {
    numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
      std::fprintf(stderr, "Invalid arguments: # of threads not specified "
                           "and unable to determine hardware concurrency.\n");
      return false;
    }
  }
  return true;
}
}
