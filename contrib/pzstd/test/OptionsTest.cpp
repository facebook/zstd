/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#include "Options.h"

#include <gtest/gtest.h>
#include <array>

using namespace pzstd;

namespace pzstd {
bool operator==(const Options& lhs, const Options& rhs) {
  return lhs.numThreads == rhs.numThreads &&
      lhs.maxWindowLog == rhs.maxWindowLog &&
      lhs.compressionLevel == rhs.compressionLevel &&
      lhs.decompress == rhs.decompress && lhs.inputFile == rhs.inputFile &&
      lhs.outputFile == rhs.outputFile && lhs.overwrite == rhs.overwrite &&
      lhs.pzstdHeaders == rhs.pzstdHeaders;
}
}

TEST(Options, ValidInputs) {
  {
    Options options;
    std::array<const char*, 6> args = {
        {nullptr, "--num-threads", "5", "-o", "-", "-f"}};
    EXPECT_TRUE(options.parse(args.size(), args.data()));
    Options expected = {5, 23, 3, false, "-", "-", true, false};
    EXPECT_EQ(expected, options);
  }
  {
    Options options;
    std::array<const char*, 6> args = {
        {nullptr, "-n", "1", "input", "-19", "-p"}};
    EXPECT_TRUE(options.parse(args.size(), args.data()));
    Options expected = {1, 23, 19, false, "input", "input.zst", false, true};
    EXPECT_EQ(expected, options);
  }
  {
    Options options;
    std::array<const char*, 10> args = {{nullptr,
                                         "--ultra",
                                         "-22",
                                         "-n",
                                         "1",
                                         "--output",
                                         "x",
                                         "-d",
                                         "x.zst",
                                         "-f"}};
    EXPECT_TRUE(options.parse(args.size(), args.data()));
    Options expected = {1, 0, 22, true, "x.zst", "x", true, false};
    EXPECT_EQ(expected, options);
  }
  {
    Options options;
    std::array<const char*, 6> args = {{nullptr,
                                        "--num-threads",
                                        "100",
                                        "hello.zst",
                                        "--decompress",
                                        "--force"}};
    EXPECT_TRUE(options.parse(args.size(), args.data()));
    Options expected = {100, 23, 3, true, "hello.zst", "hello", true, false};
    EXPECT_EQ(expected, options);
  }
  {
    Options options;
    std::array<const char*, 5> args = {{nullptr, "-", "-n", "1", "-c"}};
    EXPECT_TRUE(options.parse(args.size(), args.data()));
    Options expected = {1, 23, 3, false, "-", "-", false, false};
    EXPECT_EQ(expected, options);
  }
  {
    Options options;
    std::array<const char*, 5> args = {{nullptr, "-", "-n", "1", "--stdout"}};
    EXPECT_TRUE(options.parse(args.size(), args.data()));
    Options expected = {1, 23, 3, false, "-", "-", false, false};
    EXPECT_EQ(expected, options);
  }
  {
    Options options;
    std::array<const char*, 10> args = {{nullptr,
                                         "-n",
                                         "1",
                                         "-",
                                         "-5",
                                         "-o",
                                         "-",
                                         "-u",
                                         "-d",
                                         "--pzstd-headers"}};
    EXPECT_TRUE(options.parse(args.size(), args.data()));
    Options expected = {1, 0, 5, true, "-", "-", false, true};
  }
  {
    Options options;
    std::array<const char*, 6> args = {
        {nullptr, "silesia.tar", "-o", "silesia.tar.pzstd", "-n", "2"}};
    EXPECT_TRUE(options.parse(args.size(), args.data()));
    Options expected = {
        2, 23, 3, false, "silesia.tar", "silesia.tar.pzstd", false, false};
  }
}

TEST(Options, BadNumThreads) {
  {
    Options options;
    std::array<const char*, 3> args = {{nullptr, "-o", "-"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
  {
    Options options;
    std::array<const char*, 5> args = {{nullptr, "-n", "0", "-o", "-"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
  {
    Options options;
    std::array<const char*, 4> args = {{nullptr, "-n", "-o", "-"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
}

TEST(Options, BadCompressionLevel) {
  {
    Options options;
    std::array<const char*, 3> args = {{nullptr, "x", "-20"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
  {
    Options options;
    std::array<const char*, 4> args = {{nullptr, "x", "-u", "-23"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
}

TEST(Options, InvalidOption) {
  {
    Options options;
    std::array<const char*, 3> args = {{nullptr, "x", "-x"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
}

TEST(Options, BadOutputFile) {
  {
    Options options;
    std::array<const char*, 5> args = {{nullptr, "notzst", "-d", "-n", "1"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
  {
    Options options;
    std::array<const char*, 3> args = {{nullptr, "-n", "1"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
  {
    Options options;
    std::array<const char*, 4> args = {{nullptr, "-", "-n", "1"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
}

TEST(Options, Extras) {
  {
    Options options;
    std::array<const char*, 2> args = {{nullptr, "-h"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
  {
    Options options;
    std::array<const char*, 2> args = {{nullptr, "-V"}};
    EXPECT_FALSE(options.parse(args.size(), args.data()));
  }
}
