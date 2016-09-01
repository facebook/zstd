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

using namespace pzstd;

int main(int argc, const char** argv) {
  Options options;
  if (!options.parse(argc, argv)) {
    return 1;
  }

  ErrorHolder errorHolder;
  pzstdMain(options, errorHolder);

  if (errorHolder.hasError()) {
    std::fprintf(stderr, "Error: %s.\n", errorHolder.getError().c_str());
    return 1;
  }
  return 0;
}
