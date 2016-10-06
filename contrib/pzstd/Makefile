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

.PHONY: default all test clean test32 googletest googletest32

default: pzstd

all: pzstd


$(ZSTDDIR)/libzstd.a: $(ZSTD_FILES)
	$(MAKE) -C $(ZSTDDIR) libzstd

Pzstd.o: Pzstd.h Pzstd.cpp ErrorHolder.h utils/*.h
	$(CXX) $(FLAGS) -c Pzstd.cpp -o $@

SkippableFrame.o: SkippableFrame.h SkippableFrame.cpp utils/*.h
	$(CXX) $(FLAGS) -c SkippableFrame.cpp -o $@

Options.o: Options.h Options.cpp
	$(CXX) $(FLAGS) -c Options.cpp -o $@

main.o: main.cpp *.h utils/*.h
	$(CXX) $(FLAGS) -c main.cpp -o $@

pzstd: Pzstd.o SkippableFrame.o Options.o main.o $(ZSTDDIR)/libzstd.a
	$(CXX) $(FLAGS) $^ -o $@$(EXT) -lpthread

$(ZSTDDIR)/libzstd32.a: $(ZSTD_FILES)
	$(MAKE) -C $(ZSTDDIR) libzstd MOREFLAGS="-m32"

Pzstd32.o: Pzstd.h Pzstd.cpp ErrorHolder.h utils/*.h
	$(CXX) -m32 $(FLAGS) -c Pzstd.cpp -o $@

SkippableFrame32.o: SkippableFrame.h SkippableFrame.cpp utils/*.h
	$(CXX) -m32 $(FLAGS) -c SkippableFrame.cpp -o $@

Options32.o: Options.h Options.cpp
	$(CXX) -m32 $(FLAGS) -c Options.cpp -o $@

main32.o: main.cpp *.h utils/*.h
	$(CXX) -m32 $(FLAGS) -c main.cpp -o $@

pzstd32: Pzstd32.o SkippableFrame32.o Options32.o main32.o libzstd32.a
	$(CXX) -m32 $(FLAGS) $^ -o $@$(EXT) -lpthread

googletest:
	@$(RM) -rf googletest
	@git clone https://github.com/google/googletest
	@mkdir -p googletest/build
	@cd googletest/build && cmake .. && make

googletest32:
	@$(RM) -rf googletest
	@git clone https://github.com/google/googletest
	@mkdir -p googletest/build
	@cd googletest/build && cmake .. -DCMAKE_CXX_FLAGS=-m32 && make

googletest-mingw64:
	$(RM) -rf googletest
	git clone https://github.com/google/googletest
	mkdir -p googletest/build
	cd googletest/build && cmake -G "MSYS Makefiles" .. && $(MAKE)

test:
	$(MAKE) $(ZSTDDIR)/libzstd.a
	$(MAKE) pzstd MOREFLAGS="-Wall -Wextra -pedantic -Werror"
	$(MAKE) -C utils/test clean
	$(MAKE) -C utils/test test MOREFLAGS="-Wall -Wextra -pedantic -Werror"
	$(MAKE) -C test clean
	$(MAKE) -C test test MOREFLAGS="-Wall -Wextra -pedantic -Werror"

test32:
	$(MAKE) $(ZSTDDIR)/libzstd.a MOREFLAGS="-m32"
	$(MAKE) pzstd MOREFLAGS="-m32 -Wall -Wextra -pedantic -Werror"
	$(MAKE) -C utils/test clean
	$(MAKE) -C utils/test test MOREFLAGS="-m32 -Wall -Wextra -pedantic -Werror"
	$(MAKE) -C test clean
	$(MAKE) -C test test MOREFLAGS="-m32 -Wall -Wextra -pedantic -Werror"


clean:
	$(MAKE) -C $(ZSTDDIR) clean
	$(MAKE) -C utils/test clean
	$(MAKE) -C test clean
	@$(RM) -rf $(ZSTDDIR)/libzstd.a *.o pzstd$(EXT) pzstd32$(EXT)
	@echo Cleaning completed
