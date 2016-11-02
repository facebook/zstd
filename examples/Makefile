# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ################################################################

# This Makefile presumes libzstd is installed, using `sudo make install`

LDFLAGS+= -lzstd

.PHONY: default all clean test

default: all

all: simple_compression simple_decompression \
	dictionary_compression dictionary_decompression \
	streaming_compression streaming_decompression \
	multiple_streaming_compression

simple_compression : simple_compression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

simple_decompression : simple_decompression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

dictionary_compression : dictionary_compression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

dictionary_decompression : dictionary_decompression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

streaming_compression : streaming_compression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

multiple_streaming_compression : multiple_streaming_compression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

streaming_decompression : streaming_decompression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	@rm -f core *.o tmp* result* *.zst \
        simple_compression simple_decompression \
        dictionary_compression dictionary_decompression \
        streaming_compression streaming_decompression \
		multiple_streaming_compression
	@echo Cleaning completed

test: all
	cp README.md tmp
	cp Makefile tmp2
	@echo starting simple compression
	./simple_compression tmp
	./simple_decompression tmp.zst
	./streaming_decompression tmp.zst > /dev/null
	@echo starting streaming compression
	./streaming_compression tmp
	./streaming_decompression tmp.zst > /dev/null
	@echo starting multiple streaming compression
	./multiple_streaming_compression *.c
	@echo starting dictionary compression
	./dictionary_compression tmp2 tmp README.md
	./dictionary_decompression tmp2.zst tmp.zst README.md
	$(RM) tmp* *.zst
	@echo tests completed
