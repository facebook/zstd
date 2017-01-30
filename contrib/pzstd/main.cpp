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

using namespace pzstd;

int main(int argc, const char** argv) {
  Options options;
  switch (options.parse(argc, argv)) {
  case Options::Status::Failure:
    return 1;
  case Options::Status::Message:
    return 0;
  default:
    break;
  }

  return pzstdMain(options);
}
