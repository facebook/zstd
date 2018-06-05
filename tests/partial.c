/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#include <stdio.h>
#include "zstd_errors.h"
//#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#define ZBUFF_DISABLE_DEPRECATE_WARNINGS
#define ZBUFF_STATIC_LINKING_ONLY
#include "zbuff.h"
#define ZDICT_DISABLE_DEPRECATE_WARNINGS
#define ZDICT_STATIC_LINKING_ONLY
#include "zdict.h"

/*
size_t ZSTD_compress(void* dst, size_t dstCapacity, const void* src, size_t srcSize, int compressionLevel) __attribute__((weak)); //compress
size_t ZSTD_decompress( void* dst, size_t dstCapacity, const void* src, size_t compressedSize) __attribute__((weak)); //decompress
int weakfunc() __attribute__((weak)); //deprecated
int weakfunc() __attribute__((weak)); //dictbuilder
int weakfunc() __attribute__((weak)); //legacy
*/
//size_t ZSTD_compress(void* dst, size_t dstCapacity, const void* src, size_t srcSize, int compressionLevel) __attribute__((weak)); //compress
//size_t ZSTD_decompress( void* dst, size_t dstCapacity, const void* src, size_t compressedSize) __attribute__((weak)); //decompress
unsigned ZBUFF_isError(size_t errorCode) __attribute__((weak)); //deprecated
unsigned ZDICT_isError(size_t errorCode) __attribute__((weak));
unsigned ZBUFFv07_isError(size_t errorCode) __attribute__((weak));
unsigned ZBUFFv06_isError(size_t errorCode) __attribute__((weak));
unsigned ZBUFFv05_isError(size_t errorCode) __attribute__((weak));
unsigned ZBUFFv04_isError(size_t errorCode) __attribute__((weak));
unsigned ZBUFFv03_isError(size_t errorCode) __attribute__((weak));
unsigned ZBUFFv02_isError(size_t errorCode) __attribute__((weak));
unsigned ZBUFFv01_isError(size_t errorCode) __attribute__((weak));

int checkCompress(void) {
	if(ZSTD_compress) {
		return 1;
	} else {
		return 0;
	}
}

int checkDecompress(void) {
	if(ZSTD_decompress) {
		return 1;
	} else {
		return 0;
	}
}

int checkDeprecated(void) {
	if(ZBUFF_isError) {
			return 1;
	} else {
		return 0;
	}
}

int checkDictBuilder(void) {
	if(ZDICT_isError) {
			return 1;
	} else {
		return 0;
	}
}

int checkLegacy(void) {
	if(ZBUFFv01_isError) {
		return 1;
	} else if (ZBUFFv02_isError) {
		return 2;
	} else if (ZBUFFv03_isError) {
		return 3;
	} else if (ZBUFFv04_isError) {
		return 4;
	} else if (ZBUFFv05_isError) {
		return 5;
	} else if (ZBUFFv06_isError) {
		return 6;
	} else if (ZBUFFv07_isError) {
		return 7;
	} 
	return 0;
}

int main(void){//int argc, const char** argv) {
  //const void **symbol;
  //(void)argc;
  //(void)argv;

  printf("%d %d %d %d %d\n", checkCompress(), checkDecompress(), checkDeprecated(), checkDictBuilder(), checkLegacy());
  return 0;
}
