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

PZSTDDIR = ../..

# Set GTEST_INC and GTEST_LIB to work with your install of gtest
GTEST_INC ?= -isystem $(PZSTDDIR)/googletest/googletest/include
GTEST_LIB ?= -L $(PZSTDDIR)/googletest/build/googlemock/gtest

CPPFLAGS = -I$(PZSTDDIR) $(GTEST_INC) $(GTEST_LIB)
CXXFLAGS  ?= -O3
CXXFLAGS  += -std=c++11
CFLAGS  += $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS)

%: %.cpp
	$(CXX) $(FLAGS) $^ -o $@$(EXT) -lgtest -lgtest_main -lpthread

.PHONY: test clean

test: BufferTest RangeTest ScopeGuardTest ThreadPoolTest WorkQueueTest
	@./BufferTest$(EXT)
	@./RangeTest$(EXT)
	@./ScopeGuardTest$(EXT)
	@./ThreadPoolTest$(EXT)
	@./WorkQueueTest$(EXT)

clean:
	@rm -f BufferTest RangeTest ScopeGuardTest ThreadPoolTest WorkQueueTest
