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

CPPFLAGS += -I.. -I../../seekable_format -I../../../lib -I../../../lib/common

CFLAGS ?= -O3
CFLAGS += -g

DIS_OBJS = ../zstddis_compress.c ../zstddis_decompress.c
SEEKABLE_OBJS = ../../seekable_format/zstdseek_compress.c ../../seekable_format/zstdseek_decompress.c

.PHONY: default all clean test

default: all

all: seekable_compression seekable_decompression

$(ZSTDLIB):
	make -C $(ZSTDLIB_PATH) $(ZSTDLIB_NAME)

seekable_compression : seekable_compression.c $(DIS_OBJS) $(SEEKABLE_OBJS) $(ZSTDLIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

seekable_decompression : seekable_decompression.c $(DIS_OBJS) $(SEEKABLE_OBJS) $(ZSTDLIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	@rm -f core *.o tmp* result* *.zst \
		seekable_compression seekable_decompression
	@echo Cleaning completed
