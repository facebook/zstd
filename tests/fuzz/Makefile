# ################################################################
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

# Optionally user defined flags
CFLAGS ?= -O3
CXXFLAGS ?= -O3
CPPFLAGS ?=
LDFLAGS ?=
ARFLAGS ?=
LIB_FUZZING_ENGINE ?= libregression.a

ZSTDDIR = ../../lib
PRGDIR = ../../programs

FUZZ_CPPFLAGS := -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/compress \
	-I$(ZSTDDIR)/dictBuilder -I$(ZSTDDIR)/deprecated -I$(PRGDIR) \
	$(CPPFLAGS)
FUZZ_EXTRA_FLAGS := -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
	-Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
	-Wstrict-prototypes -Wundef -Wformat-security \
	-Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
	-Wredundant-decls \
	-g -fno-omit-frame-pointer
FUZZ_CFLAGS := $(FUZZ_EXTRA_FLAGS) $(CFLAGS)
FUZZ_CXXFLAGS := $(FUZZ_EXTRA_FLAGS) -std=c++11 $(CXXFLAGS)
FUZZ_LDFLAGS := $(LDFLAGS)
FUZZ_ARFLAGS := $(ARFLAGS)
FUZZ_TARGET_FLAGS = $(FUZZ_CPPFLAGS) $(FUZZ_CXXFLAGS) $(FUZZ_LDFLAGS)

FUZZ_HEADERS := fuzz_helpers.h fuzz.h zstd_helpers.h
FUZZ_SRC := zstd_helpers.c

ZSTDCOMMON_SRC := $(ZSTDDIR)/common/*.c
ZSTDCOMP_SRC   := $(ZSTDDIR)/compress/*.c
ZSTDDECOMP_SRC := $(ZSTDDIR)/decompress/*.c
FUZZ_SRC       := \
	$(FUZZ_SRC) \
	$(ZSTDDECOMP_SRC) \
	$(ZSTDCOMMON_SRC) \
	$(ZSTDCOMP_SRC)

FUZZ_OBJ := $(patsubst %.c,%.o, $(wildcard $(FUZZ_SRC)))


.PHONY: default all clean

default: all

all: \
	simple_round_trip \
	stream_round_trip \
	block_round_trip \
	simple_decompress \
	stream_decompress \
	block_decompress

%.o: %.c
	$(CC) $(FUZZ_CPPFLAGS) $(FUZZ_CFLAGS) $^ -c -o $@

simple_round_trip: $(FUZZ_HEADERS) $(FUZZ_OBJ) simple_round_trip.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(FUZZ_OBJ) simple_round_trip.o $(LIB_FUZZING_ENGINE) -o $@

stream_round_trip: $(FUZZ_HEADERS) $(FUZZ_OBJ) stream_round_trip.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(FUZZ_OBJ) stream_round_trip.o $(LIB_FUZZING_ENGINE) -o $@

block_round_trip: $(FUZZ_HEADERS) $(FUZZ_OBJ) block_round_trip.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(FUZZ_OBJ) block_round_trip.o $(LIB_FUZZING_ENGINE) -o $@

simple_decompress: $(FUZZ_HEADERS) $(FUZZ_OBJ) simple_decompress.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(FUZZ_OBJ) simple_decompress.o $(LIB_FUZZING_ENGINE) -o $@

stream_decompress: $(FUZZ_HEADERS) $(FUZZ_OBJ) stream_decompress.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(FUZZ_OBJ) stream_decompress.o $(LIB_FUZZING_ENGINE) -o $@

block_decompress: $(FUZZ_HEADERS) $(FUZZ_OBJ) block_decompress.o
	$(CXX) $(FUZZ_TARGET_FLAGS) $(FUZZ_OBJ) block_decompress.o $(LIB_FUZZING_ENGINE) -o $@

libregression.a: $(FUZZ_HEADERS) $(PRGDIR)/util.h regression_driver.o
	$(AR) $(FUZZ_ARFLAGS) $@ regression_driver.o

# Install libfuzzer (not usable for MSAN testing)
# Provided for convienence. To use this library run make libFuzzer and
# set LDFLAGS=-L.
.PHONY: libFuzzer
libFuzzer:
	@$(RM) -rf Fuzzer
	@git clone https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer
	@cd Fuzzer && ./build.sh

clean:
	@$(MAKE) -C $(ZSTDDIR) clean
	@$(RM) -f *.a *.o
	@$(RM) -f simple_round_trip stream_round_trip simple_decompress stream_decompress
