# ################################################################
# Copyright (c) 2017-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

# This Makefile presumes libzstd is built, using `make` in / or /lib/

ZSTDLIB_PATH = ../../../lib
ZSTDLIB_NAME = libzstd.a
ZSTDLIB = $(ZSTDLIB_PATH)/$(ZSTDLIB_NAME)

CPPFLAGS += -I../ -I../../../lib -I../../../lib/common

CFLAGS ?= -O3
CFLAGS += -g

SEEKABLE_OBJS = ../zstdseek_compress.c ../zstdseek_decompress.c $(ZSTDLIB)

.PHONY: default clean test

default: test

test: seekable_tests

$(ZSTDLIB):
	make -C $(ZSTDLIB_PATH) $(ZSTDLIB_NAME)

seekable_tests : seekable_tests.c $(SEEKABLE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	@rm -f core *.o tmp* result* *.zst \
		seekable_tests
	@echo Cleaning completed
