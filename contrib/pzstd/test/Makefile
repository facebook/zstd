# ##########################################################################
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ##########################################################################

# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif

PZSTDDIR = ..
PROGDIR = ../../../programs
ZSTDDIR = ../../../lib

# Set GTEST_INC and GTEST_LIB to work with your install of gtest
GTEST_INC ?= -isystem $(PZSTDDIR)/googletest/googletest/include
GTEST_LIB ?= -L $(PZSTDDIR)/googletest/build/googlemock/gtest

CPPFLAGS = -I$(PZSTDDIR) $(GTEST_INC) $(GTEST_LIB) -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(PROGDIR) -I.

CXXFLAGS  ?= -O3
CXXFLAGS  += -std=c++11
CXXFLAGS  += $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS)

datagen.o: $(PROGDIR)/datagen.*
	$(CXX) $(FLAGS) $(PROGDIR)/datagen.c -c -o $@

%: %.cpp *.h datagen.o
	$(CXX) $(FLAGS) $@.cpp datagen.o $(PZSTDDIR)/Pzstd.o $(PZSTDDIR)/SkippableFrame.o $(PZSTDDIR)/Options.o $(PZSTDDIR)/libzstd.a -o $@$(EXT) -lgtest -lgtest_main -lpthread

.PHONY: test clean

test: OptionsTest PzstdTest
	@./OptionsTest$(EXT)
	@./PzstdTest$(EXT)

roundtrip: RoundTripTest
	@./RoundTripTest$(EXT)

clean:
	@rm -f datagen.o OptionsTest PzstdTest RoundTripTest
