/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#include "datagen.h"
#include "Pzstd.h"
#include "test/RoundTrip.h"
#include "utils/ScopeGuard.h"

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdio>
#include <memory>

using namespace std;
using namespace pzstd;

TEST(Pzstd, SmallSizes) {
  for (unsigned len = 1; len < 1028; ++len) {
    std::string inputFile = std::tmpnam(nullptr);
    auto guard = makeScopeGuard([&] { std::remove(inputFile.c_str()); });
    {
      static uint8_t buf[1028];
      RDG_genBuffer(buf, len, 0.5, 0.0, 42);
      auto fd = std::fopen(inputFile.c_str(), "wb");
      auto written = std::fwrite(buf, 1, len, fd);
      std::fclose(fd);
      ASSERT_EQ(written, len);
    }
    for (unsigned headers = 0; headers <= 1; ++headers) {
      for (unsigned numThreads = 1; numThreads <= 4; numThreads *= 2) {
        for (unsigned level = 1; level <= 8; level *= 8) {
          auto errorGuard = makeScopeGuard([&] {
            guard.dismiss();
            std::fprintf(stderr, "file: %s\n", inputFile.c_str());
            std::fprintf(stderr, "pzstd headers: %u\n", headers);
            std::fprintf(stderr, "# threads: %u\n", numThreads);
            std::fprintf(stderr, "compression level: %u\n", level);
          });
          Options options;
          options.pzstdHeaders = headers;
          options.overwrite = true;
          options.inputFile = inputFile;
          options.numThreads = numThreads;
          options.compressionLevel = level;
          ASSERT_TRUE(roundTrip(options));
          errorGuard.dismiss();
        }
      }
    }
  }
}

TEST(Pzstd, LargeSizes) {
  for (unsigned len = 1 << 20; len <= (1 << 24); len *= 2) {
    std::string inputFile = std::tmpnam(nullptr);
    auto guard = makeScopeGuard([&] { std::remove(inputFile.c_str()); });
    {
      std::unique_ptr<uint8_t[]> buf(new uint8_t[len]);
      RDG_genBuffer(buf.get(), len, 0.5, 0.0, 42);
      auto fd = std::fopen(inputFile.c_str(), "wb");
      auto written = std::fwrite(buf.get(), 1, len, fd);
      std::fclose(fd);
      ASSERT_EQ(written, len);
    }
    for (unsigned headers = 0; headers <= 1; ++headers) {
      for (unsigned numThreads = 1; numThreads <= 16; numThreads *= 4) {
        for (unsigned level = 1; level <= 4; level *= 2) {
          auto errorGuard = makeScopeGuard([&] {
            guard.dismiss();
            std::fprintf(stderr, "file: %s\n", inputFile.c_str());
            std::fprintf(stderr, "pzstd headers: %u\n", headers);
            std::fprintf(stderr, "# threads: %u\n", numThreads);
            std::fprintf(stderr, "compression level: %u\n", level);
          });
          Options options;
          options.pzstdHeaders = headers;
          options.overwrite = true;
          options.inputFile = inputFile;
          options.numThreads = numThreads;
          options.compressionLevel = level;
          ASSERT_TRUE(roundTrip(options));
          errorGuard.dismiss();
        }
      }
    }
  }
}

TEST(Pzstd, ExtremelyCompressible) {
  std::string inputFile = std::tmpnam(nullptr);
  auto guard = makeScopeGuard([&] { std::remove(inputFile.c_str()); });
  {
    std::unique_ptr<uint8_t[]> buf(new uint8_t[10000]);
    std::memset(buf.get(), 'a', 10000);
    auto fd = std::fopen(inputFile.c_str(), "wb");
    auto written = std::fwrite(buf.get(), 1, 10000, fd);
    std::fclose(fd);
    ASSERT_EQ(written, 10000);
  }
  Options options;
  options.pzstdHeaders = false;
  options.overwrite = true;
  options.inputFile = inputFile;
  options.numThreads = 1;
  options.compressionLevel = 1;
  ASSERT_TRUE(roundTrip(options));
}
