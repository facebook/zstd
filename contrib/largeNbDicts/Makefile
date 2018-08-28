# ################################################################
# Copyright (c) 2018-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################


CPPFLAGS+= -I../../lib -I../../lib/common -I../../lib/dictBuilder -I../../programs

CFLAGS  ?= -O3
DEBUGFLAGS= -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
            -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wundef -Wpointer-arith -Wformat-security \
            -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            -Wredundant-decls
CFLAGS  += $(DEBUGFLAGS) $(MOREFLAGS)


default: largeNbDicts

all : largeNbDicts

largeNbDicts: LDFLAGS += -lzstd
largeNbDicts: largeNbDicts.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@


clean:
	$(RM) largeNbDicts
