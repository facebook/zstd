# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
# ################################################################

LINUX := ../linux
LINUX_ZSTDLIB := $(LINUX)/lib/zstd

CPPFLAGS += -I$(LINUX)/include -I$(LINUX_ZSTDLIB) -Iinclude -DNDEBUG -Wno-deprecated-declarations
# Don't poison the workspace, it currently doesn't work with static allocation and workspace reuse
CPPFLAGS += -DZSTD_ASAN_DONT_POISON_WORKSPACE

LINUX_ZSTD_MODULE     := $(wildcard $(LINUX_ZSTDLIB)/*.c)
LINUX_ZSTD_COMMON     := $(wildcard $(LINUX_ZSTDLIB)/common/*.c)
LINUX_ZSTD_COMPRESS   := $(wildcard $(LINUX_ZSTDLIB)/compress/*.c)
LINUX_ZSTD_DECOMPRESS := $(wildcard $(LINUX_ZSTDLIB)/decompress/*.c $(LINUX_ZSTDLIB)/decompress/*.S)
LINUX_ZSTD_FILES      := $(LINUX_ZSTD_MODULE) $(LINUX_ZSTD_COMMON) $(LINUX_ZSTD_COMPRESS) $(LINUX_ZSTD_DECOMPRESS)
LINUX_ZSTD_OBJECTS0   := $(LINUX_ZSTD_FILES:.c=.o)
LINUX_ZSTD_OBJECTS    := $(LINUX_ZSTD_OBJECTS0:.S=.o)

X86_BUILD_DIR              := x86
X86_LINUX_ZSTD_FILES       := $(patsubst $(LINUX_ZSTDLIB)/%,%,$(LINUX_ZSTD_FILES))
X86_LINUX_ZSTD_OBJECTS0    := $(patsubst %.c,$(X86_BUILD_DIR)/%.o,$(X86_LINUX_ZSTD_FILES))
X86_LINUX_ZSTD_OBJECTS     := $(patsubst %.S,$(X86_BUILD_DIR)/%.o,$(X86_LINUX_ZSTD_OBJECTS0))
X86_LINUX_ZSTD_CPPFLAGS    := -D__KERNEL__ -DCONFIG_X86

%.o: %.S
	$(COMPILE.S) $(OUTPUT_OPTION) $<

$(X86_LINUX_ZSTD_OBJECTS): CPPFLAGS += $(X86_LINUX_ZSTD_CPPFLAGS)

$(X86_BUILD_DIR)/%.o: $(LINUX_ZSTDLIB)/%.c
	@mkdir -p $(@D)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(X86_BUILD_DIR)/%.o: $(LINUX_ZSTDLIB)/%.S
	@mkdir -p $(@D)
	$(COMPILE.S) $(OUTPUT_OPTION) $<

liblinuxzstd.a: $(LINUX_ZSTD_OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

liblinuxzstd_x86.a: $(X86_LINUX_ZSTD_OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

test: test.c liblinuxzstd.a
	$(CC) $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $^ -o $@

test_x86: test.c liblinuxzstd_x86.a
	$(CC) $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $^ -o $@

static_test: static_test.c
	$(CC) $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $^ -o $@

run-test: test test_x86 static_test
	./macro-test.sh
	./test
	./test_x86
	./static_test

.PHONY:
clean:
	$(RM) -f $(LINUX_ZSTDLIB)/*.o
	$(RM) -f $(LINUX_ZSTDLIB)/**/*.o
	$(RM) -rf $(X86_BUILD_DIR)
	$(RM) -f *.o *.a
	$(RM) -f static_test
	$(RM) -f test test_x86
