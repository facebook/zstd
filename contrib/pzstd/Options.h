/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#pragma once

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#undef ZSTD_STATIC_LINKING_ONLY

#include <cstdint>
#include <string>

namespace pzstd {

struct Options {
  unsigned numThreads;
  unsigned maxWindowLog;
  unsigned compressionLevel;
  bool decompress;
  std::string inputFile;
  std::string outputFile;
  bool overwrite;
  bool pzstdHeaders;

  Options();
  Options(
      unsigned numThreads,
      unsigned maxWindowLog,
      unsigned compressionLevel,
      bool decompress,
      const std::string& inputFile,
      const std::string& outputFile,
      bool overwrite,
      bool pzstdHeaders)
      : numThreads(numThreads),
        maxWindowLog(maxWindowLog),
        compressionLevel(compressionLevel),
        decompress(decompress),
        inputFile(inputFile),
        outputFile(outputFile),
        overwrite(overwrite),
        pzstdHeaders(pzstdHeaders) {}

  bool parse(int argc, const char** argv);

  ZSTD_parameters determineParameters() const {
    ZSTD_parameters params = ZSTD_getParams(compressionLevel, 0, 0);
    if (maxWindowLog != 0 && params.cParams.windowLog > maxWindowLog) {
      params.cParams.windowLog = maxWindowLog;
      params.cParams = ZSTD_adjustCParams(params.cParams, 0, 0);
    }
    return params;
  }
};
}
