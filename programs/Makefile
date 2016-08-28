# ##########################################################################
# ZSTD programs - Makefile
# Copyright (C) Yann Collet 2015-2016
#
# GPL v2 License
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# You can contact the author at :
#  - zstd homepage : http://www.zstd.net/
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

CPPFLAGS= -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/dictBuilder
CFLAGS ?= -O3
CFLAGS += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 \
          -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes -Wundef
CFLAGS += $(MOREFLAGS)
FLAGS   = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)


ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES := $(ZSTDDIR)/compress/zstd_compress.c $(ZSTDDIR)/compress/fse_compress.c $(ZSTDDIR)/compress/huf_compress.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/huf_decompress.c
ZSTD_FILES := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)
ZDICT_FILES := $(ZSTDDIR)/dictBuilder/*.c
ZSTDDECOMP_O = $(ZSTDDIR)/decompress/zstd_decompress.o
ZSTDDECOMP32_O = $(ZSTDDIR)/decompress/zstd_decompress32.o

ifeq ($(ZSTD_LEGACY_SUPPORT), 0)
CPPFLAGS  += -DZSTD_LEGACY_SUPPORT=0
ZSTDLEGACY_FILES:=
else
ZSTD_LEGACY_SUPPORT:=1
CPPFLAGS  += -I$(ZSTDDIR)/legacy -I./legacy
ZSTDLEGACY_FILES:= $(ZSTDDIR)/legacy/*.c legacy/fileio_legacy.c
endif


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
VOID = nul
else
EXT =
VOID = /dev/null
endif


.PHONY: default all clean install uninstall

default: zstd

all: zstd


$(ZSTDDECOMP_O): $(ZSTDDIR)/decompress/zstd_decompress.c
	$(CC)    $(ALIGN_LOOP) $(FLAGS) -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT) $^ -c -o $@

$(ZSTDDECOMP32_O): $(ZSTDDIR)/decompress/zstd_decompress.c
	$(CC)  -m32 $(ALIGN_LOOP) $(FLAGS) -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT) $^ -c -o $@

zstd  : $(ZSTDDECOMP_O) $(ZSTD_FILES) $(ZSTDLEGACY_FILES) $(ZDICT_FILES) \
        zstdcli.c fileio.c bench.c datagen.c dibio.c
	$(CC)      $(FLAGS) -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT) $^ -o $@$(EXT)

zstd32 : $(ZSTDDECOMP32_O) $(ZSTD_FILES) $(ZSTDLEGACY_FILES) $(ZDICT_FILES) \
        zstdcli.c fileio.c bench.c datagen.c dibio.c
	$(CC)  -m32 $(FLAGS) -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT) $^ -o $@$(EXT)


zstd_nolegacy :
	$(MAKE) zstd ZSTD_LEGACY_SUPPORT=0

zstd-pgo : MOREFLAGS = -fprofile-generate
zstd-pgo : clean zstd
	./zstd -b19i1 $(PROFILE_WITH)
	./zstd -b16i1 $(PROFILE_WITH)
	./zstd -b9i2 $(PROFILE_WITH)
	./zstd -b $(PROFILE_WITH)
	./zstd -b7i2 $(PROFILE_WITH)
	./zstd -b5 $(PROFILE_WITH)
	rm zstd
	$(MAKE) zstd MOREFLAGS=-fprofile-use

zstd-frugal: $(ZSTDDECOMP_O) $(ZSTD_FILES) zstdcli.c fileio.c
	$(CC)      $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_LEGACY_SUPPORT=0 $^ -o zstd$(EXT)

zstd-compress: $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES) \
	zstdcli.c fileio.c
	$(CC)      $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NODECOMPRESS -DZSTD_LEGACY_SUPPORT=0 $^ -o $@$(EXT)

zstd-decompress: $(ZSTDCOMMON_FILES) $(ZSTDDECOMP_FILES) \
	zstdcli.c fileio.c
	$(CC)      $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NOCOMPRESS -DZSTD_LEGACY_SUPPORT=0 $^ -o $@$(EXT)

zstd-small: clean
	CFLAGS="-Os -s" $(MAKE) zstd-frugal


clean:
	$(MAKE) -C ../lib clean
	@rm -f ../lib/decompress/*.o
	@rm -f core *.o tmp* result* *.gcda dictionary *.zst \
        zstd$(EXT) zstd32$(EXT) zstd-compress$(EXT) zstd-decompress$(EXT)
	@echo Cleaning completed


#----------------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD, Hurd and some BSD targets
#----------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD DragonFly))
install: zstd
	@echo Installing binaries
	@install -d -m 755 $(DESTDIR)$(BINDIR)/ $(DESTDIR)$(MANDIR)/
	@install -m 755 zstd$(EXT) $(DESTDIR)$(BINDIR)/zstd$(EXT)
	@ln -sf zstd$(EXT) $(DESTDIR)$(BINDIR)/zstdcat
	@ln -sf zstd$(EXT) $(DESTDIR)$(BINDIR)/unzstd
	@echo Installing man pages
	@install -m 644 zstd.1 $(DESTDIR)$(MANDIR)/zstd.1
	@ln -sf zstd.1 $(DESTDIR)$(MANDIR)/zstdcat.1
	@ln -sf zstd.1 $(DESTDIR)$(MANDIR)/unzstd.1
	@echo zstd installation completed

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/zstdcat
	rm -f $(DESTDIR)$(BINDIR)/unzstd
	[ -x $(DESTDIR)$(BINDIR)/zstd$(EXT) ] && rm -f $(DESTDIR)$(BINDIR)/zstd$(EXT)
	rm -f $(DESTDIR)$(MANDIR)/zstdcat.1
	rm -f $(DESTDIR)$(MANDIR)/unzstd.1
	[ -f $(DESTDIR)$(MANDIR)/zstd.1 ] && rm -f $(DESTDIR)$(MANDIR)/zstd.1
	@echo zstd programs successfully uninstalled
endif
