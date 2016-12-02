# ##########################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
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

DESTDIR?=
PREFIX ?= /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man/man1

ZSTDDIR = ../lib

ifeq ($(shell $(CC) -v 2>&1 | grep -c "gcc version "), 1)
ALIGN_LOOP = -falign-loops=32
else
ALIGN_LOOP =
endif

CPPFLAGS+= -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/dictBuilder
CFLAGS  ?= -O3
CFLAGS  += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 \
          -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes -Wundef \
          -Wpointer-arith
CFLAGS  += $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)


ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES := $(ZSTDDIR)/compress/zstd_compress.c $(ZSTDDIR)/compress/fse_compress.c $(ZSTDDIR)/compress/huf_compress.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/huf_decompress.c
ZSTD_FILES := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)
ZDICT_FILES := $(ZSTDDIR)/dictBuilder/*.c
ZSTDDECOMP_O = $(ZSTDDIR)/decompress/zstd_decompress.o

ifeq ($(ZSTD_LEGACY_SUPPORT), 0)
CPPFLAGS  += -DZSTD_LEGACY_SUPPORT=0
ZSTDLEGACY_FILES:=
else
ZSTD_LEGACY_SUPPORT:=1
CPPFLAGS  += -I$(ZSTDDIR)/legacy
ZSTDLEGACY_FILES:= $(ZSTDDIR)/legacy/*.c
endif


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
RES64_FILE = windres\zstd64.res
RES32_FILE = windres\zstd32.res
ifneq (,$(filter x86_64%,$(shell $(CC) -dumpmachine)))
    RES_FILE = $(RES64_FILE)
else
    RES_FILE = $(RES32_FILE)
endif
else
EXT =
endif


.PHONY: default all clean clean_decomp_o install uninstall generate_res

default: zstd

all: zstd

$(ZSTDDECOMP_O): CFLAGS += $(ALIGN_LOOP)

zstd  : CPPFLAGS += -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT)
zstd  : $(ZSTDDECOMP_O) $(ZSTD_FILES) $(ZSTDLEGACY_FILES) $(ZDICT_FILES) \
        zstdcli.c fileio.c bench.c datagen.c dibio.c
ifneq (,$(filter Windows%,$(OS)))
	windres\generate_res.bat
endif
	$(CC) $(FLAGS) $^ $(RES_FILE) -o $@$(EXT) $(LDFLAGS)


zstd32 : CPPFLAGS += -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT)
zstd32 : $(ZSTDDIR)/decompress/zstd_decompress.c $(ZSTD_FILES) $(ZSTDLEGACY_FILES) $(ZDICT_FILES) \
        zstdcli.c fileio.c bench.c datagen.c dibio.c
ifneq (,$(filter Windows%,$(OS)))
	windres\generate_res.bat
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

zstd-frugal: $(ZSTDDECOMP_O) $(ZSTD_FILES) zstdcli.c fileio.c
	$(CC) $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT $^ -o zstd$(EXT)

zstd-small: clean_decomp_o
	ZSTD_LEGACY_SUPPORT=0 CFLAGS="-Os -s" $(MAKE) zstd-frugal

zstd-decompress-clean: $(ZSTDDECOMP_O) $(ZSTDCOMMON_FILES) $(ZSTDDECOMP_FILES) zstdcli.c fileio.c
	$(CC) $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NOCOMPRESS $^ -o zstd-decompress$(EXT)

zstd-decompress: clean_decomp_o
	ZSTD_LEGACY_SUPPORT=0 $(MAKE) zstd-decompress-clean

zstd-compress: $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES) zstdcli.c fileio.c
	$(CC) $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NODECOMPRESS $^ -o $@$(EXT)

gzstd: clean_decomp_o
ifeq ($(shell ld -lz 2>/dev/null && echo -n true),true)
	CPPFLAGS=-DZSTD_GZDECOMPRESS LDFLAGS="-lz" $(MAKE) zstd
else
	$(MAKE) zstd
endif

generate_res:
	windres\generate_res.bat

clean:
	$(MAKE) -C ../lib clean
	@$(RM) $(ZSTDDIR)/decompress/*.o $(ZSTDDIR)/decompress/zstd_decompress.gcda
	@$(RM) core *.o tmp* result* *.gcda dictionary *.zst \
        zstd$(EXT) zstd32$(EXT) zstd-compress$(EXT) zstd-decompress$(EXT) \
        *.gcda default.profraw
	@echo Cleaning completed

clean_decomp_o:
	@$(RM) $(ZSTDDECOMP_O)


#----------------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD, Hurd and some BSD targets
#----------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD DragonFly NetBSD))
install: zstd
	@echo Installing binaries
	@install -d -m 755 $(DESTDIR)$(BINDIR)/ $(DESTDIR)$(MANDIR)/
	@install -m 755 zstd $(DESTDIR)$(BINDIR)/zstd
	@ln -sf zstd $(DESTDIR)$(BINDIR)/zstdcat
	@ln -sf zstd $(DESTDIR)$(BINDIR)/unzstd
	@install -m 755 zstdless $(DESTDIR)$(BINDIR)/zstdless
	@install -m 755 zstdgrep $(DESTDIR)$(BINDIR)/zstdgrep
	@echo Installing man pages
	@install -m 644 zstd.1 $(DESTDIR)$(MANDIR)/zstd.1
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
