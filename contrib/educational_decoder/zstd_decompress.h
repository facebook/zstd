/*
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

size_t ZSTD_decompress(void *const dst, const size_t dst_len,
                       const void *const src, const size_t src_len);
size_t ZSTD_decompress_with_dict(void *const dst, const size_t dst_len,
                                 const void *const src, const size_t src_len,
                                 const void *const dict, const size_t dict_len);
size_t ZSTD_get_decompressed_size(const void *const src, const size_t src_len);

