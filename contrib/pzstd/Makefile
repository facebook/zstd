# ##########################################################################
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ##########################################################################

ZSTDDIR = ../../lib
PROGDIR = ../../programs

CPPFLAGS = -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(PROGDIR) -I.
CXXFLAGS  ?= -O3
CXXFLAGS  += -std=c++11
CXXFLAGS  += $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS)


ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES := $(ZSTDDIR)/compress/zstd_compress.c $(ZSTDDIR)/compress/fse_compress.c $(ZSTDDIR)/compress/huf_compress.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/huf_decompress.c
ZSTD_FILES := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif

.PHONY: default all test clean

default: pzstd

all: pzstd


libzstd.a: $(ZSTD_FILES)
	$(MAKE) -C $(ZSTDDIR) libzstd
	@cp $(ZSTDDIR)/libzstd.a .


Pzstd.o: Pzstd.h Pzstd.cpp ErrorHolder.h utils/*.h
	$(CXX) $(FLAGS) -c Pzstd.cpp -o $@

SkippableFrame.o: SkippableFrame.h SkippableFrame.cpp utils/*.h
	$(CXX) $(FLAGS) -c SkippableFrame.cpp -o $@

Options.o: Options.h Options.cpp
	$(CXX) $(FLAGS) -c Options.cpp -o $@

main.o: main.cpp *.h utils/*.h
	$(CXX) $(FLAGS) -c main.cpp -o $@

pzstd: Pzstd.o SkippableFrame.o Options.o main.o libzstd.a 
	$(CXX) $(FLAGS) $^ -o $@$(EXT) -lpthread

googletest:
	@git clone https://github.com/google/googletest
	@mkdir -p googletest/build
	@cd googletest/build && cmake .. && make

test: libzstd.a Pzstd.o Options.o SkippableFrame.o
	$(MAKE) -C utils/test clean
	$(MAKE) -C utils/test test
	$(MAKE) -C test clean
	$(MAKE) -C test test

clean:
	$(MAKE) -C $(ZSTDDIR) clean
	$(MAKE) -C utils/test clean
	$(MAKE) -C test clean
	@$(RM) -rf libzstd.a *.o pzstd$(EXT)
	@echo Cleaning completed
