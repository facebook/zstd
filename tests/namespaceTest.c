/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
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
