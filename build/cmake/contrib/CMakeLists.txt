# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

project(contrib
  LANGUAGES C   # Main library is in C
  ASM # And ASM
  CXX # pzstd, gen_html
)

add_subdirectory(pzstd)
add_subdirectory(gen_html)
