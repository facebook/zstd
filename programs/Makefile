# ##########################################################################
# Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This Makefile is validated for Linux, macOS, *BSD, Hurd, Solaris, MSYS2 targets
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ##########################################################################
# zstd : Command Line Utility, supporting gzip-like arguments
# zstd32 : Same as zstd, but forced to compile in 32-bits mode
# zstd_nolegacy : zstd without support of decompression of legacy versions
# zstd-small : minimal zstd without dictionary builder and benchmark
# zstd-compress : compressor-only version of zstd
# zstd-decompress : decompressor-only version of zstd
# ##########################################################################

ZSTDDIR = ../lib

# Version numbers
LIBVER_SRC := $(ZSTDDIR)/zstd.h
LIBVER_MAJOR_SCRIPT:=`sed -n '/define ZSTD_VERSION_MAJOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < $(LIBVER_SRC)`
LIBVER_MINOR_SCRIPT:=`sed -n '/define ZSTD_VERSION_MINOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < $(LIBVER_SRC)`
LIBVER_PATCH_SCRIPT:=`sed -n '/define ZSTD_VERSION_RELEASE/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < $(LIBVER_SRC)`
LIBVER_SCRIPT:= $(LIBVER_MAJOR_SCRIPT).$(LIBVER_MINOR_SCRIPT).$(LIBVER_PATCH_SCRIPT)
LIBVER_MAJOR := $(shell echo $(LIBVER_MAJOR_SCRIPT))
LIBVER_MINOR := $(shell echo $(LIBVER_MINOR_SCRIPT))
LIBVER_PATCH := $(shell echo $(LIBVER_PATCH_SCRIPT))
LIBVER  := $(shell echo $(LIBVER_SCRIPT))

ZSTD_VERSION=$(LIBVER)

ifeq ($(shell $(CC) -v 2>&1 | grep -c "gcc version "), 1)
ALIGN_LOOP = -falign-loops=32
else
ALIGN_LOOP =
endif

CPPFLAGS+= -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/compress \
           -I$(ZSTDDIR)/dictBuilder \
           -DXXH_NAMESPACE=ZSTD_   # because xxhash.o already compiled with this macro from library
CFLAGS  ?= -O3
DEBUGFLAGS = -g -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
          -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
          -Wstrict-prototypes -Wundef -Wpointer-arith -Wformat-security
CFLAGS  += $(DEBUGFLAGS) $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)


ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES := $(ZSTDDIR)/compress/*.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/*.c
ZSTD_FILES := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)
ZDICT_FILES := $(ZSTDDIR)/dictBuilder/*.c
ZSTDDECOMP_O = $(ZSTDDIR)/decompress/zstd_decompress.o

ZSTD_LEGACY_SUPPORT ?= 4
ZSTDLEGACY_FILES:=
ifneq ($(ZSTD_LEGACY_SUPPORT), 0)
ifeq ($(shell test $(ZSTD_LEGACY_SUPPORT) -lt 8; echo $$?), 0)
	ZSTDLEGACY_FILES += $(shell ls $(ZSTDDIR)/legacy/*.c | grep 'v0[$(ZSTD_LEGACY_SUPPORT)-7]')
endif
	CPPFLAGS += -I$(ZSTDDIR)/legacy
else
endif

ZSTDLIB_FILES := $(wildcard $(ZSTD_FILES)) $(wildcard $(ZSTDLEGACY_FILES)) $(wildcard $(ZDICT_FILES))
ZSTDLIB_OBJ   := $(patsubst %.c,%.o,$(ZSTDLIB_FILES))

# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
RES64_FILE = windres/zstd64.res
RES32_FILE = windres/zstd32.res
ifneq (,$(filter x86_64%,$(shell $(CC) -dumpmachine)))
    RES_FILE = $(RES64_FILE)
else
    RES_FILE = $(RES32_FILE)
endif
else
EXT =
endif

VOID = /dev/null

# thread detection
NO_THREAD_MSG := ==> no threads, building without multithreading support
HAVE_PTHREAD := $(shell printf '\#include <pthread.h>\nint main(void) { return 0; }' | $(CC) $(FLAGS) -o have_pthread$(EXT) -x c - -pthread 2> $(VOID) && rm have_pthread$(EXT) && echo 1 || echo 0)
HAVE_THREAD := $(shell [ "$(HAVE_PTHREAD)" -eq "1" -o -n "$(filter Windows%,$(OS))" ] && echo 1 || echo 0)
ifeq ($(HAVE_THREAD), 1)
THREAD_MSG := ==> building with threading support
THREAD_CPP := -DZSTD_MULTITHREAD
THREAD_LD := -pthread
else
THREAD_MSG := $(NO_THREAD_MSG)
endif

# zlib detection
NO_ZLIB_MSG := ==> no zlib, building zstd without .gz support
HAVE_ZLIB := $(shell printf '\#include <zlib.h>\nint main(void) { return 0; }' | $(CC) $(FLAGS) -o have_zlib$(EXT) -x c - -lz 2> $(VOID) && rm have_zlib$(EXT) && echo 1 || echo 0)
ifeq ($(HAVE_ZLIB), 1)
ZLIB_MSG := ==> building zstd with .gz compression support
ZLIBCPP = -DZSTD_GZCOMPRESS -DZSTD_GZDECOMPRESS
ZLIBLD = -lz
else
ZLIB_MSG := $(NO_ZLIB_MSG)
endif

# lzma detection
NO_LZMA_MSG := ==> no liblzma, building zstd without .xz/.lzma support
HAVE_LZMA := $(shell printf '\#include <lzma.h>\nint main(void) { return 0; }' | $(CC) $(FLAGS) -o have_lzma$(EXT) -x c - -llzma 2> $(VOID) && rm have_lzma$(EXT) && echo 1 || echo 0)
ifeq ($(HAVE_LZMA), 1)
LZMA_MSG := ==> building zstd with .xz/.lzma compression support
LZMACPP = -DZSTD_LZMACOMPRESS -DZSTD_LZMADECOMPRESS
LZMALD = -llzma
else
LZMA_MSG := $(NO_LZMA_MSG)
endif

# lz4 detection
NO_LZ4_MSG := ==> no liblz4, building zstd without .lz4 support
HAVE_LZ4 := $(shell printf '\#include <lz4frame.h>\n\#include <lz4.h>\nint main(void) { return 0; }' | $(CC) $(FLAGS) -o have_lz4$(EXT) -x c - -llz4 2> $(VOID) && rm have_lz4$(EXT) && echo 1 || echo 0)
ifeq ($(HAVE_LZ4), 1)
LZ4_MSG := ==> building zstd with .lz4 compression support
LZ4CPP = -DZSTD_LZ4COMPRESS -DZSTD_LZ4DECOMPRESS
LZ4LD = -llz4
else
LZ4_MSG := $(NO_LZ4_MSG)
endif

.PHONY: default all clean clean_decomp_o install uninstall generate_res

default: zstd-release

all: zstd

$(ZSTDDECOMP_O): CFLAGS += $(ALIGN_LOOP)

zstd xzstd zstd4 xzstd4 : CPPFLAGS += $(THREAD_CPP) $(ZLIBCPP)
zstd xzstd zstd4 xzstd4 : LDFLAGS += $(THREAD_LD) $(ZLIBLD)
xzstd xzstd4 : CPPFLAGS += $(LZMACPP)
xzstd xzstd4 : LDFLAGS += $(LZMALD)
zstd4 xzstd4 : CPPFLAGS += $(LZ4CPP)
zstd4 xzstd4 : LDFLAGS += $(LZ4LD)
zstd zstd4 : LZMA_MSG := - xz/lzma support is disabled
zstd xzstd : LZ4_MSG := - lz4 support is disabled
zstd xzstd zstd4 xzstd4 : CPPFLAGS += -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT)
zstd xzstd zstd4 xzstd4 : $(ZSTDLIB_FILES) zstdcli.o fileio.o bench.o datagen.o dibio.o
	@echo "$(THREAD_MSG)"
	@echo "$(ZLIB_MSG)"
	@echo "$(LZMA_MSG)"
	@echo "$(LZ4_MSG)"
ifneq (,$(filter Windows%,$(OS)))
	windres/generate_res.bat
endif
	$(CC) $(FLAGS) $^ $(RES_FILE) -o zstd$(EXT) $(LDFLAGS)

zstd-release: DEBUGFLAGS :=
zstd-release: zstd

zstd32 : CPPFLAGS += -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT)
zstd32 : $(ZSTDLIB_FILES) zstdcli.c fileio.c bench.c datagen.c dibio.c
ifneq (,$(filter Windows%,$(OS)))
	windres/generate_res.bat
endif
	$(CC) -m32 $(FLAGS) $^ $(RES32_FILE) -o $@$(EXT)

zstd-nolegacy : clean_decomp_o
	$(MAKE) zstd ZSTD_LEGACY_SUPPORT=0

zstd-nomt : THREAD_CPP :=
zstd-nomt : THREAD_LD :=
zstd-nomt : THREAD_MSG := - multi-threading disabled
zstd-nomt : zstd

zstd-nogz : ZLIBCPP :=
zstd-nogz : ZLIBLD :=
zstd-nogz : ZLIB_MSG := - gzip support is disabled
zstd-nogz : zstd


zstd-pgo : MOREFLAGS = -fprofile-generate
zstd-pgo : clean zstd
	./zstd -b19i1 $(PROFILE_WITH)
	./zstd -b16i1 $(PROFILE_WITH)
	./zstd -b9i2 $(PROFILE_WITH)
	./zstd -b $(PROFILE_WITH)
	./zstd -b7i2 $(PROFILE_WITH)
	./zstd -b5 $(PROFILE_WITH)
	$(RM) zstd
	$(RM) $(ZSTDDECOMP_O)
	$(MAKE) zstd MOREFLAGS=-fprofile-use

# minimal target, with only zstd compression and decompression. no bench. no legacy.
zstd-small: CFLAGS = "-Os -s"
zstd-frugal zstd-small: $(ZSTD_FILES) zstdcli.c fileio.c
	$(CC) $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT $^ -o zstd$(EXT)

zstd-decompress: $(ZSTDCOMMON_FILES) $(ZSTDDECOMP_FILES) zstdcli.c fileio.c
	$(CC) $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NOCOMPRESS $^ -o $@$(EXT)

zstd-compress: $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES) zstdcli.c fileio.c
	$(CC) $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NODECOMPRESS $^ -o $@$(EXT)

# zstd is now built with Multi-threading by default
zstdmt: zstd

generate_res:
	windres/generate_res.bat

clean:
	$(MAKE) -C $(ZSTDDIR) clean
	@$(RM) $(ZSTDDIR)/decompress/*.o $(ZSTDDIR)/decompress/zstd_decompress.gcda
	@$(RM) core *.o tmp* result* *.gcda dictionary *.zst \
        zstd$(EXT) zstd32$(EXT) zstd-compress$(EXT) zstd-decompress$(EXT) \
        *.gcda default.profraw have_zlib$(EXT)
	@echo Cleaning completed

clean_decomp_o:
	@$(RM) $(ZSTDDECOMP_O)

MD2ROFF = ronn
MD2ROFF_FLAGS = --roff --warnings --manual="User Commands" --organization="zstd $(ZSTD_VERSION)"

zstd.1: zstd.1.md
	cat $^ | $(MD2ROFF) $(MD2ROFF_FLAGS) | sed -n '/^\.\\\".*/!p' > $@

man: zstd.1

clean-man:
	rm zstd.1

preview-man: clean-man man
	man ./zstd.1

#-----------------------------------------------------------------------------
# make install is validated only for Linux, OSX, BSD, Hurd and Solaris targets
#-----------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS))

ifneq (,$(filter $(shell uname),SunOS))
INSTALL ?= ginstall
else
INSTALL ?= install
endif

PREFIX  ?= /usr/local
DESTDIR ?=
BINDIR  ?= $(PREFIX)/bin

ifneq (,$(filter $(shell uname),OpenBSD FreeBSD NetBSD DragonFly SunOS))
MANDIR  ?= $(PREFIX)/man/man1
else
MANDIR  ?= $(PREFIX)/share/man/man1
endif

INSTALL_PROGRAM ?= $(INSTALL) -m 755
INSTALL_SCRIPT  ?= $(INSTALL) -m 755
INSTALL_MAN     ?= $(INSTALL) -m 644

install: zstd
	@echo Installing binaries
	@$(INSTALL) -d -m 755 $(DESTDIR)$(BINDIR)/ $(DESTDIR)$(MANDIR)/
	@$(INSTALL_PROGRAM) zstd $(DESTDIR)$(BINDIR)/zstd
	@ln -sf zstd $(DESTDIR)$(BINDIR)/zstdcat
	@ln -sf zstd $(DESTDIR)$(BINDIR)/unzstd
	@ln -sf zstd $(DESTDIR)$(BINDIR)/zstdmt
	@$(INSTALL_SCRIPT) zstdless $(DESTDIR)$(BINDIR)/zstdless
	@$(INSTALL_SCRIPT) zstdgrep $(DESTDIR)$(BINDIR)/zstdgrep
	@echo Installing man pages
	@$(INSTALL_MAN) zstd.1 $(DESTDIR)$(MANDIR)/zstd.1
	@ln -sf zstd.1 $(DESTDIR)$(MANDIR)/zstdcat.1
	@ln -sf zstd.1 $(DESTDIR)$(MANDIR)/unzstd.1
	@echo zstd installation completed

uninstall:
	@$(RM) $(DESTDIR)$(BINDIR)/zstdgrep
	@$(RM) $(DESTDIR)$(BINDIR)/zstdless
	@$(RM) $(DESTDIR)$(BINDIR)/zstdcat
	@$(RM) $(DESTDIR)$(BINDIR)/unzstd
	@$(RM) $(DESTDIR)$(BINDIR)/zstd
	@$(RM) $(DESTDIR)$(MANDIR)/zstdcat.1
	@$(RM) $(DESTDIR)$(MANDIR)/unzstd.1
	@$(RM) $(DESTDIR)$(MANDIR)/zstd.1
	@echo zstd programs successfully uninstalled
endif
