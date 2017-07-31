# ################################################################
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ################################################################

# This Makefile presumes libzstd is installed, using `sudo make install`

CPPFLAGS+= -I../../lib/common
CFLAGS  ?= -O3
DEBUGFLAGS = -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
            -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wundef -Wpointer-arith -Wformat-security \
            -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            -Wredundant-decls
CFLAGS  += $(DEBUGFLAGS) $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CFLAGS)

LDFLAGS += -lzstd

.PHONY: default all clean

default: all

all: ldm 
	
ldm: ldm_common.c ldm.c main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	@rm -f core *.o tmp* result* *.ldm *.ldm.dec \
	ldm
	@echo Cleaning completed

