# ##########################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ##########################################################################
# datagen : Synthetic and parametrable data generator, for tests
# fullbench  : Precisely measure speed for each zstd inner functions
# fullbench32: Same as fullbench, but forced to compile in 32-bits mode
# fuzzer  : Test tool, to check zstd integrity on target platform
# fuzzer32: Same as fuzzer, but forced to compile in 32-bits mode
# paramgrill : parameter tester for zstd
# test-zstd-speed.py : script for testing zstd speed difference between commits
# versionsTest : compatibility test between zstd versions stored on Github (v0.1+)
# zbufftest  : Test tool, to check ZBUFF integrity on target platform
# zbufftest32: Same as zbufftest, but forced to compile in 32-bits mode
# zstreamtest : Fuzzer test tool for zstd streaming API
# zbufftest32: Same as zstreamtest, but forced to compile in 32-bits mode
# ##########################################################################

DESTDIR?=
PREFIX ?= /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man/man1
ZSTDDIR = ../lib
PRGDIR  = ../programs
PYTHON ?= python3
TESTARTEFACT := versionsTest namespaceTest


CPPFLAGS= -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/dictBuilder -I$(PRGDIR)
CFLAGS ?= -O3
CFLAGS += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 \
          -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes -Wundef
CFLAGS += $(MOREFLAGS)
FLAGS   = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)


ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES := $(ZSTDDIR)/compress/zstd_compress.c $(ZSTDDIR)/compress/fse_compress.c $(ZSTDDIR)/compress/huf_compress.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/zstd_decompress.c $(ZSTDDIR)/decompress/huf_decompress.c
ZSTD_FILES := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)
ZBUFF_FILES := $(ZSTDDIR)/compress/zbuff_compress.c $(ZSTDDIR)/decompress/zbuff_decompress.c
ZDICT_FILES := $(ZSTDDIR)/dictBuilder/*.c



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

.PHONY: default all all32 clean test test32 test-all namespaceTest versionsTest

default: fullbench

all: fullbench fuzzer zbufftest zstreamtest paramgrill datagen

all32: fullbench32 fuzzer32 zbufftest32 zstreamtest32



zstd:
	$(MAKE) -C $(PRGDIR) $@

zstd32:
	$(MAKE) -C $(PRGDIR) $@

zstd-nolegacy:
	$(MAKE) -C $(PRGDIR) $@

fullbench  : $(ZSTD_FILES) $(PRGDIR)/datagen.c fullbench.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

fullbench32 : $(ZSTD_FILES) $(PRGDIR)/datagen.c fullbench.c
	$(CC)  -m32  $(FLAGS) $^ -o $@$(EXT)

fuzzer  : CPPFLAGS += -I$(ZSTDDIR)/dictBuilder
fuzzer  : $(ZSTD_FILES) $(ZDICT_FILES) $(PRGDIR)/datagen.c fuzzer.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

fuzzer32 : CPPFLAGS += -I$(ZSTDDIR)/dictBuilder
fuzzer32 : $(ZSTD_FILES) $(ZDICT_FILES) $(PRGDIR)/datagen.c fuzzer.c
	$(CC)  -m32  $(FLAGS) $^ -o $@$(EXT)

zbufftest  : $(ZSTD_FILES) $(ZBUFF_FILES) $(PRGDIR)/datagen.c zbufftest.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

zbufftest32  : $(ZSTD_FILES) $(ZBUFF_FILES) $(PRGDIR)/datagen.c zbufftest.c
	$(CC)  -m32  $(FLAGS) $^ -o $@$(EXT)

zstreamtest  : $(ZSTD_FILES) $(PRGDIR)/datagen.c zstreamtest.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

zstreamtest32 : $(ZSTD_FILES) $(PRGDIR)/datagen.c zstreamtest.c
	$(CC) -m32  $(FLAGS) $^ -o $@$(EXT)

paramgrill : $(ZSTD_FILES) $(PRGDIR)/datagen.c paramgrill.c
	$(CC)      $(FLAGS) $^ -lm -o $@$(EXT)

datagen : $(PRGDIR)/datagen.c datagencli.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

roundTripCrash : $(ZSTD_FILES) roundTripCrash.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

namespaceTest:
	if $(CC) namespaceTest.c ../lib/common/xxhash.c -o $@ ; then echo compilation should fail; exit 1 ; fi
	$(RM) $@

versionsTest:
	$(PYTHON) test-zstd-versions.py

clean:
	$(MAKE) -C ../lib clean
	@$(RM) -fR $(TESTARTEFACT)
	@$(RM) -f core *.o tmp* result* *.gcda dictionary *.zst \
        $(PRGDIR)/zstd$(EXT) $(PRGDIR)/zstd32$(EXT) \
        fullbench$(EXT) fullbench32$(EXT) \
        fuzzer$(EXT) fuzzer32$(EXT) zbufftest$(EXT) zbufftest32$(EXT) \
		zstreamtest$(EXT) zstreamtest32$(EXT) \
        datagen$(EXT) paramgrill$(EXT) roundTripCrash$(EXT)
	@echo Cleaning completed


#----------------------------------------------------------------------------------
#make valgrindTest is validated only for Linux, OSX, kFreeBSD, Hurd and some BSD targets
#----------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD DragonFly))
HOST_OS = POSIX

valgrindTest: VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrindTest: zstd datagen fuzzer fullbench zbufftest
	@echo "\n ---- valgrind tests : memory analyzer ----"
	$(VALGRIND) ./datagen -g50M > $(VOID)
	$(VALGRIND) $(PRGDIR)/zstd ; if [ $$? -eq 0 ] ; then echo "zstd without argument should have failed"; false; fi
	./datagen -g80 | $(VALGRIND) $(PRGDIR)/zstd - -c > $(VOID)
	./datagen -g16KB | $(VALGRIND) $(PRGDIR)/zstd -vf - -c > $(VOID)
	./datagen -g2930KB | $(VALGRIND) $(PRGDIR)/zstd -5 -vf - -o tmp
	$(VALGRIND) $(PRGDIR)/zstd -vdf tmp -c > $(VOID)
	./datagen -g64MB | $(VALGRIND) $(PRGDIR)/zstd -vf - -c > $(VOID)
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

test: test-zstd test-fullbench test-fuzzer test-zbuff test-zstream

test32: test-zstd32 test-fullbench32 test-fuzzer32 test-zbuff32 test-zstream32

test-all: test test32 valgrindTest

test-zstd: ZSTD = $(PRGDIR)/zstd
test-zstd: zstd zstd-playTests

test-zstd32: ZSTD = $(PRGDIR)/zstd32
test-zstd32: zstd32 zstd-playTests

test-zstd-nolegacy: ZSTD = $(PRGDIR)/zstd
test-zstd-nolegacy: zstd-nolegacy zstd-playTests

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

test-zstream: zstreamtest
	./zstreamtest $(ZBUFFTEST)

test-zstream32: zstreamtest32
	./zstreamtest32 $(ZBUFFTEST)

endif
