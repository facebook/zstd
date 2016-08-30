/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */



#include <stddef.h>  /* size_t */
#include <string.h>  /* strlen */

/* symbol definition */
extern unsigned XXH32(const void* src, size_t srcSize, unsigned seed);

int main(int argc, const char** argv)
{
    const char* exename = argv[0];
    unsigned result = XXH32(exename, strlen(exename), argc);
    return !result;
}
