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

# zlib detection
NO_ZLIB_MSG := ==> no zlib, building zstd without .gz support
VOID = /dev/null
HAVE_ZLIB := $(shell printf '\#include <zlib.h>\nint main(){}' | $(CC) -o have_zlib -x c - -lz 2> $(VOID) && rm have_zlib$(EXT) && echo 1 || echo 0)
ifeq ($(HAVE_ZLIB), 1)
ZLIB_MSG := ==> building zstd with .gz compression support
ZLIBCPP = -DZSTD_GZCOMPRESS -DZSTD_GZDECOMPRESS
ZLIBLD = -lz
else
ZLIB_MSG := $(NO_ZLIB_MSG)
endif
# lzma detection
NO_LZMA_MSG := ==> no liblzma, building zstd without .xz/.lzma support
HAVE_LZMA := $(shell printf '\#include <lzma.h>\nint main(){}' | $(CC) -o have_lzma -x c - -llzma 2> $(VOID) && rm have_lzma$(EXT) && echo 1 || echo 0)
ifeq ($(HAVE_LZMA), 1)
LZMA_MSG := ==> building zstd with .xz/.lzma compression support
LZMACPP = -DZSTD_LZMACOMPRESS -DZSTD_LZMADECOMPRESS
LZMALD = -llzma
else
LZMA_MSG := $(NO_LZMA_MSG)
endif


.PHONY: default all clean clean_decomp_o install uninstall generate_res

default: zstd-release

all: zstd

$(ZSTDDECOMP_O): CFLAGS += $(ALIGN_LOOP)

zstd : CPPFLAGS += $(ZLIBCPP)
zstd : LDFLAGS += $(ZLIBLD)
zstd : LZMA_MSG := $(NO_LZMA_MSG)
zstd-nogz : ZLIB_MSG := $(NO_ZLIB_MSG)
zstd-nogz : LZMA_MSG := $(NO_LZMA_MSG)
xzstd : CPPFLAGS += $(ZLIBCPP) $(LZMACPP)
xzstd : LDFLAGS += $(ZLIBLD) $(LZMALD)
zstd zstd-nogz xzstd : CPPFLAGS += -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT)
zstd zstd-nogz xzstd : $(ZSTDLIB_OBJ) zstdcli.o fileio.o bench.o datagen.o dibio.o
	@echo "$(ZLIB_MSG)"
	@echo "$(LZMA_MSG)"
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

zstd-frugal: $(ZSTD_FILES) zstdcli.c fileio.c
	$(CC) $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT $^ -o zstd$(EXT)

zstd-small:
	CFLAGS="-Os -s" $(MAKE) zstd-frugal

zstd-decompress: $(ZSTDCOMMON_FILES) $(ZSTDDECOMP_FILES) zstdcli.c fileio.c
	$(CC) $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NOCOMPRESS $^ -o $@$(EXT)

zstd-compress: $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES) zstdcli.c fileio.c
	$(CC) $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NODECOMPRESS $^ -o $@$(EXT)

zstdmt: CPPFLAGS += -DZSTD_MULTITHREAD
ifeq (,$(filter Windows%,$(OS)))
zstdmt: LDFLAGS += -lpthread
endif
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
