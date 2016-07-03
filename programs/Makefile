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
# datagen : Synthetic and parametrable data generator, for tests
# fuzzer  : Test tool, to check zstd integrity on target platform
# fuzzer32: Same as fuzzer, but forced to compile in 32-bits mode
# zbufftest  : Test tool, to check ZBUFF integrity on target platform
# zbufftest32: Same as zbufftest, but forced to compile in 32-bits mode
# fullbench  : Precisely measure speed for each zstd inner function
# fullbench32: Same as fullbench, but forced to compile in 32-bits mode
# ##########################################################################

DESTDIR?=
PREFIX ?= /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man/man1

ZSTDDIR = ../lib

CPPFLAGS= -I$(ZSTDDIR)/common -I$(ZSTDDIR)/dictBuilder -DXXH_NAMESPACE=ZSTD_
CFLAGS ?= -O3  # -falign-loops=32   # not always beneficial
CFLAGS += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes -Wundef
FLAGS   = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(MOREFLAGS)


ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES := $(ZSTDDIR)/compress/zstd_compress.c $(ZSTDDIR)/compress/fse_compress.c $(ZSTDDIR)/compress/huf_compress.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/huf_decompress.c $(ZSTDDIR)/decompress/zstd_decompress.c
ZSTD_FILES := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)
ZBUFF_FILES := $(ZSTDDIR)/compress/zbuff_compress.c $(ZSTDDIR)/decompress/zbuff_decompress.c
ZDICT_FILES := $(ZSTDDIR)/dictBuilder/*.c

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

ZBUFFTEST = -T2mn
FUZZERTEST= -T5mn
ZSTDRTTEST= --test-large-data

.PHONY: default all clean install uninstall test test32 test-all

default: zstd

all: zstd fullbench fuzzer zbufftest paramgrill datagen zstd32 fullbench32 fuzzer32 zbufftest32

zstd  : $(ZSTD_FILES) $(ZSTDLEGACY_FILES) $(ZBUFF_FILES) $(ZDICT_FILES) \
        zstdcli.c fileio.c bench.c datagen.c dibio.c
	$(CC)      $(FLAGS) -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT) $^ -o $@$(EXT)

zstd32: $(ZSTD_FILES) $(ZSTDLEGACY_FILES) $(ZBUFF_FILES) $(ZDICT_FILES) \
        zstdcli.c fileio.c bench.c datagen.c dibio.c
	$(CC) -m32 $(FLAGS) -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT) $^ -o $@$(EXT)

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

zstd-frugal: $(ZSTD_FILES) $(ZBUFF_FILES) zstdcli.c fileio.c
	$(CC)      $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_LEGACY_SUPPORT=0 $^ -o zstd$(EXT)

zstd-compress: $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES) \
	$(ZSTDDIR)/compress/zbuff_compress.c zstdcli.c fileio.c
	$(CC)      $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NODECOMPRESS -DZSTD_LEGACY_SUPPORT=0 $^ -o $@$(EXT)

zstd-decompress: $(ZSTDCOMMON_FILES) $(ZSTDDECOMP_FILES) \
	$(ZSTDDIR)/decompress/zbuff_decompress.c zstdcli.c fileio.c
	$(CC)      $(FLAGS) -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NOCOMPRESS -DZSTD_LEGACY_SUPPORT=0 $^ -o $@$(EXT)

zstd-small: clean
	CFLAGS="-Os -s" $(MAKE) zstd-frugal

fullbench  : $(ZSTD_FILES) $(ZBUFF_FILES) datagen.c fullbench.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

fullbench32: $(ZSTD_FILES) $(ZBUFF_FILES) datagen.c fullbench.c
	$(CC) -m32 $(FLAGS) $^ -o $@$(EXT)

fuzzer  : CPPFLAGS += -I$(ZSTDDIR)/dictBuilder
fuzzer  : $(ZSTD_FILES) $(ZDICT_FILES) datagen.c fuzzer.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

fuzzer32 : CPPFLAGS += -I$(ZSTDDIR)/dictBuilder
fuzzer32: $(ZSTD_FILES) $(ZDICT_FILES) datagen.c fuzzer.c
	$(CC) -m32 $(FLAGS) $^ -o $@$(EXT)

zbufftest  : $(ZSTD_FILES) $(ZBUFF_FILES) datagen.c zbufftest.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

zbufftest32: $(ZSTD_FILES) $(ZBUFF_FILES) datagen.c zbufftest.c
	$(CC) -m32 $(FLAGS) $^ -o $@$(EXT)

paramgrill : $(ZSTD_FILES) datagen.c paramgrill.c
	$(CC)      $(FLAGS) $^ -lm -o $@$(EXT)

datagen : datagen.c datagencli.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

roundTripCrash : $(ZSTD_FILES) roundTripCrash.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

clean:
	@rm -f core *.o tmp* result* *.gcda dictionary *.zst \
        zstd$(EXT) zstd32$(EXT) zstd-compress$(EXT) zstd-decompress$(EXT) \
        fullbench$(EXT) fullbench32$(EXT) \
        fuzzer$(EXT) fuzzer32$(EXT) zbufftest$(EXT) zbufftest32$(EXT) \
        datagen$(EXT) paramgrill$(EXT) roundTripCrash$(EXT)
	@echo Cleaning completed


#---------------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD, Hurd and OpenBSD targets
#---------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD))
HOST_OS = POSIX
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

valgrindTest: VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrindTest: zstd datagen fuzzer fullbench zbufftest
	@echo "\n ---- valgrind tests : memory analyzer ----"
	$(VALGRIND) ./datagen -g50M > $(VOID)
	$(VALGRIND) ./zstd ; if [ $$? -eq 0 ] ; then echo "zstd without argument should have failed"; false; fi
	./datagen -g80 | $(VALGRIND) ./zstd - -c > $(VOID)
	./datagen -g16KB | $(VALGRIND) ./zstd -vf - -o $(VOID)
	./datagen -g2930KB | $(VALGRIND) ./zstd -5 -vf - -o tmp
	$(VALGRIND) ./zstd -vdf tmp -o $(VOID)
	./datagen -g64MB | $(VALGRIND) ./zstd -vf - -o $(VOID)
	@rm tmp
	$(VALGRIND) ./fuzzer -T1mn -t1
	$(VALGRIND) ./fullbench -i1
	$(VALGRIND) ./zbufftest -T1mn

endif


ifneq (,$(filter MSYS%,$(shell uname)))
HOST_OS = MSYS
endif


#------------------------------------------------------------------------
#make tests validated only for MSYS, Linux, OSX, kFreeBSD and Hurd targets
#------------------------------------------------------------------------
ifneq (,$(filter $(HOST_OS),MSYS POSIX))
zstd-playTests: datagen
	ZSTD=$(ZSTD) ./playTests.sh $(ZSTDRTTEST)

test: test-zstd test-fullbench test-fuzzer test-zbuff

test32: test-zstd32 test-fullbench32 test-fuzzer32 test-zbuff32

test-all: test test32 valgrindTest

test-zstd: ZSTD = ./zstd
test-zstd: zstd zstd-playTests

test-zstd32: ZSTD = ./zstd32
test-zstd32: zstd32 zstd-playTests

test-zstd_nolegacy: ZSTD = ./zstd
test-zstd_nolegacy: zstd_nolegacy zstd-playTests

test-fullbench: fullbench datagen
	./fullbench -i1
	./fullbench -i1 -P0

test-fullbench32: fullbench32 datagen
	./fullbench32 -i1
	./fullbench32 -i1 -P0

test-fuzzer: fuzzer
	./fuzzer $(FUZZERTEST)

test-fuzzer32: fuzzer32
	./fuzzer32 $(FUZZERTEST)

test-zbuff: zbufftest
	./zbufftest $(ZBUFFTEST)

test-zbuff32: zbufftest32
	./zbufftest32 $(ZBUFFTEST)
endif
