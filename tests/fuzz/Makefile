# ##########################################################################
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This Makefile is validated for Linux, and macOS targets
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ##########################################################################

CFLAGS ?= -O3
CXXFLAGS ?= -O3

ZSTDDIR = ../../lib
PRGDIR = ../../programs

FUZZ_CPPFLAGS := -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/compress \
	-I$(ZSTDDIR)/dictBuilder -I$(ZSTDDIR)/deprecated -I$(PRGDIR) \
	-DZSTD_DEBUG=1 -DMEM_FORCE_MEMORY_ACCESS=0 \
	-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION $(CPPFLAGS)
FUZZ_CFLAGS := -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
	-Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
	-Wstrict-prototypes -Wundef -Wformat-security \
	-Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
	-Wredundant-decls \
	-g -fno-omit-frame-pointer $(CFLAGS)
FUZZ_CXXFLAGS := -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
	-Wstrict-aliasing=1 -Wswitch-enum \
	-Wdeclaration-after-statement -Wstrict-prototypes -Wundef \
	-Wformat-security -Wvla -Wformat=2 -Winit-self -Wfloat-equal \
	-Wwrite-strings -Wredundant-decls \
	-g -fno-omit-frame-pointer -std=c++11 $(CXXFLAGS)
FUZZ_LDFLAGS := $(LDFLAGS)
FUZZ_ARFLAGS := $(ARFLAGS)
FUZZ_TARGET_FLAGS = $(FUZZ_CPPFLAGS) $(FUZZ_CXXFLAGS) $(FUZZ_LDFLAGS)

FUZZ_HEADERS := fuzz_helpers.h fuzz.h

ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES   := $(ZSTDDIR)/compress/*.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/*.c
ZSTD_FILES       := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)

ZSTD_OBJ  := $(patsubst %.c,%.o, $(wildcard $(ZSTD_FILES)))

LIBFUZZER ?= -lFuzzer

.PHONY: default all clean

default: all

all: round_trip simple_decompress

%.o: %.c
	$(CC) $(FUZZ_CPPFLAGS) $(FUZZ_CFLAGS) $^ -c -o $@

simple_round_trip: $(FUZZ_HEADERS) $(ZSTD_OBJ) simple_round_trip.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(ZSTD_OBJ) simple_round_trip.o $(LIBFUZZER) -o $@

stream_round_trip: $(FUZZ_HEADERS) $(ZSTD_OBJ) stream_round_trip.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(ZSTD_OBJ) stream_round_trip.o $(LIBFUZZER) -o $@

simple_decompress: $(FUZZ_HEADERS) $(ZSTD_OBJ) simple_decompress.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(ZSTD_OBJ) simple_decompress.o $(LIBFUZZER) -o $@

stream_decompress: $(FUZZ_HEADERS) $(ZSTD_OBJ) stream_decompress.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(ZSTD_OBJ) stream_decompress.o $(LIBFUZZER) -o $@

libregression.a: $(FUZZ_HEADERS) $(PRGDIR)/util.h regression_driver.o
	$(AR) $(FUZZ_ARFLAGS) $@ regression_driver.o

%-regression: libregression.a
	$(RM) $*
	$(MAKE) $* LDFLAGS="$(FUZZ_LDFLAGS) -L." LIBFUZZER=-lregression

%-regression-test: %-regression
	./$* corpora/$*

regression-test: \
	simple_round_trip-regression-test \
	stream_round_trip-regression-test \
	simple_decompress-regression-test \
	stream_decompress-regression-test

%-msan: clean
	$(MAKE) $* CFLAGS="-fsanitize=memory $(FUZZ_CFLAGS)" \
		CXXFLAGS="-fsanitize=memory $(FUZZ_CXXFLAGS)"

UASAN_FLAGS := -fsanitize=address,undefined -fno-sanitize-recover=undefined \
	-fno-sanitize=pointer-overflow
%-uasan: clean
	$(MAKE) $* CFLAGS="$(FUZZ_CFLAGS) $(UASAN_FLAGS)" \
		CXXFLAGS="$(FUZZ_CXXFLAGS) $(UASAN_FLAGS)"

# Install libfuzzer (not usable for MSAN testing)
# Provided for convienence. To use this library run make libFuzzer and
# set LDFLAGS=-L.
.PHONY: libFuzzer
libFuzzer:
	@$(RM) -rf Fuzzer
	@git clone https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer
	@./Fuzzer/build.sh

clean:
	@$(MAKE) -C $(ZSTDDIR) clean
	@$(RM) -f *.a *.o
	@$(RM) -f simple_round_trip stream_round_trip simple_decompress stream_decompress
