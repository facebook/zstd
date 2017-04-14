# ################################################################
# Copyright (c) 2017-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ################################################################

# This Makefile presumes libzstd is built, using `make` in / or /lib/

LDFLAGS += -L../../../lib/ -lzstd
CPPFLAGS += -I../ -I../../../lib -I../../../lib/common

CFLAGS ?= -O3
CFLAGS += -g

SEEKABLE_OBJS = ../zstdseek_compress.c ../zstdseek_decompress.c

.PHONY: default all clean test

default: all

all: seekable_compression seekable_decompression

seekable_compression : seekable_compression.c $(SEEKABLE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

seekable_decompression : seekable_decompression.c $(SEEKABLE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	@rm -f core *.o tmp* result* *.zst \
		seekable_compression seekable_decompression
	@echo Cleaning completed
