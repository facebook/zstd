# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
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

CPPFLAGS += -DXXH_NAMESPACE=ZSTD_ -I../ -I$(ZSTDLIB_PATH) -I$(ZSTDLIB_PATH)/common

CFLAGS ?= -O3
CFLAGS += -g -Wall -Wextra -Wcast-qual -Wcast-align -Wconversion \
          -Wformat=2 -Wstrict-aliasing=1

SEEKABLE_OBJS = ../zstdseek_compress.c ../zstdseek_decompress.c $(ZSTDLIB)

.PHONY: default clean test
default: test

test: seekable_tests parallel_compression_test
	./seekable_tests

$(ZSTDLIB):
	$(MAKE) -C $(ZSTDLIB_PATH) $(ZSTDLIB_NAME)

seekable_tests : $(SEEKABLE_OBJS)

EXAMPLES_PATH = ../examples
PARALLEL_COMPRESSION = $(EXAMPLES_PATH)/parallel_compression

DATAGEN_PATH = ../../../tests
DATAGEN = $(DATAGEN_PATH)/datagen

$(PARALLEL_COMPRESSION):
	$(MAKE) -C $(EXAMPLES_PATH) parallel_compression

$(DATAGEN):
	$(MAKE) -C $(DATAGEN_PATH) datagen

data.txt: $(DATAGEN)
	$(DATAGEN) -g100M > $@

parallel_compression_test: $(PARALLEL_COMPRESSION) data.txt
	ulimit -Sv 102400; $(PARALLEL_COMPRESSION) data.txt 1048576 2

.PHONY: parallel_compression_test parallel_comp

clean:
	@$(RM) core *.o tmp* result* *.zst \
		seekable_tests data.txt
	@echo Cleaning completed
