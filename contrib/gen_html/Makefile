# ##########################################################################
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ##########################################################################


CFLAGS ?= -O3
CFLAGS += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 -Wswitch-enum -Wno-comment
CFLAGS += $(MOREFLAGS)
FLAGS   = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)



# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif


.PHONY: default gen_html

default: gen_html

gen_html: gen_html.cpp
	$(CXX)      $(FLAGS) $^ -o $@$(EXT)


clean:
	@$(RM) gen_html$(EXT)
	@echo Cleaning completed
