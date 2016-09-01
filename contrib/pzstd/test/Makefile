# ##########################################################################
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ##########################################################################

# Set GTEST_INC and GTEST_LIB to work with your install of gtest
GTEST_INC ?= -isystem googletest/googletest/include
GTEST_LIB ?= -L googletest/build/googlemock/gtest

# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif

PZSTDDIR = ..
PROGDIR = ../../../programs
ZSTDDIR = ../../../lib

CPPFLAGS = -I$(PZSTDDIR) $(GTEST_INC) $(GTEST_LIB) -I$(ZSTDDIR)/common -I$(PROGDIR)

CFLAGS  ?= -O3
CFLAGS  += -std=c++11
CFLAGS  += $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

datagen.o: $(PROGDIR)/datagen.*
	$(CXX) $(FLAGS) $(PROGDIR)/datagen.c -c -o $@

%: %.cpp *.h datagen.o
	$(CXX) $(FLAGS) -lgtest -lgtest_main $@.cpp datagen.o $(PZSTDDIR)/libzstd.a $(PZSTDDIR)/Pzstd.o $(PZSTDDIR)/SkippableFrame.o $(PZSTDDIR)/Options.o -o $@$(EXT)

.PHONY: test clean

test: OptionsTest PzstdTest RoundTripTest
	@./OptionsTest$(EXT)
	@./PzstdTest$(EXT)
	@./RoundTripTest$(EXT)

clean:
	@rm -f datagen.o OptionsTest PzstdTest RoundTripTest
