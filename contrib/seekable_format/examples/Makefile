# ################################################################
# Copyright (c) 2017-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

# This Makefile presumes libzstd is built, using `make` in / or /lib/

LDFLAGS += ../../../lib/libzstd.a
CPPFLAGS += -I../ -I../../../lib -I../../../lib/common

CFLAGS ?= -O3
CFLAGS += -g

SEEKABLE_OBJS = ../zstdseek_compress.c ../zstdseek_decompress.c

.PHONY: default all clean test

default: all

all: seekable_compression seekable_decompression parallel_processing

seekable_compression : seekable_compression.c $(SEEKABLE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

seekable_decompression : seekable_decompression.c $(SEEKABLE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

parallel_processing : parallel_processing.c $(SEEKABLE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@ -pthread

parallel_compression : parallel_compression.c $(SEEKABLE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@ -pthread

clean:
	@rm -f core *.o tmp* result* *.zst \
		seekable_compression seekable_decompression \
		parallel_processing parallel_compression
	@echo Cleaning completed
