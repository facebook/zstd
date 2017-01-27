# ##########################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This Makefile is validated for Linux, macOS, *BSD, Hurd, Solaris, MSYS2 targets
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
# zstreamtest : Fuzzer test tool for zstd streaming API
# zstreamtest32: Same as zstreamtest, but forced to compile in 32-bits mode
# ##########################################################################

ZSTDDIR = ../lib
PRGDIR  = ../programs
PYTHON ?= python3
TESTARTEFACT := versionsTest namespaceTest


CPPFLAGS+= -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/compress -I$(ZSTDDIR)/dictBuilder -I$(ZSTDDIR)/deprecated -I$(PRGDIR)
CFLAGS  ?= -O3
CFLAGS  += -g -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 \
           -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes -Wundef
CFLAGS  += $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)


ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES := $(ZSTDDIR)/compress/*.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/*.c
ZSTD_FILES := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)
ZBUFF_FILES := $(ZSTDDIR)/deprecated/*.c
ZDICT_FILES := $(ZSTDDIR)/dictBuilder/*.c



# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
MULTITHREAD = -DZSTD_MULTITHREAD
else
EXT =
MULTITHREAD = -pthread -DZSTD_MULTITHREAD
endif

VOID = /dev/null
ZSTREAM_TESTTIME = -T2mn
FUZZERTEST ?= -T5mn
ZSTDRTTEST = --test-large-data

.PHONY: default all all32 dll clean test test32 test-all namespaceTest versionsTest

default: fullbench

all: fullbench fuzzer zstreamtest paramgrill datagen zbufftest

all32: fullbench32 fuzzer32 zstreamtest32 zbufftest32

dll: fuzzer-dll zstreamtest-dll zbufftest-dll



zstd:
	$(MAKE) -C $(PRGDIR) $@

zstd32:
	$(MAKE) -C $(PRGDIR) $@

zstd-nolegacy:
	$(MAKE) -C $(PRGDIR) $@

gzstd:
	$(MAKE) -C $(PRGDIR) $@

fullbench  : $(ZSTD_FILES) $(PRGDIR)/datagen.c fullbench.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

fullbench32 : $(ZSTD_FILES) $(PRGDIR)/datagen.c fullbench.c
	$(CC)  -m32  $(FLAGS) $^ -o $@$(EXT)

fullbench-lib: $(PRGDIR)/datagen.c fullbench.c
	$(MAKE) -C $(ZSTDDIR) libzstd.a
	$(CC) $(FLAGS) $^ -o $@$(EXT) $(ZSTDDIR)/libzstd.a

fullbench-dll: $(PRGDIR)/datagen.c fullbench.c
	$(MAKE) -C $(ZSTDDIR) libzstd
	$(CC) $(FLAGS) $^ -o $@$(EXT) -DZSTD_DLL_IMPORT=1 $(ZSTDDIR)/dll/libzstd.dll

fuzzer   : $(ZSTD_FILES) $(ZDICT_FILES) $(PRGDIR)/datagen.c fuzzer.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

fuzzer32 : $(ZSTD_FILES) $(ZDICT_FILES) $(PRGDIR)/datagen.c fuzzer.c
	$(CC) -m32 $(FLAGS) $^ -o $@$(EXT)

fuzzer-dll : LDFLAGS+= -L$(ZSTDDIR) -lzstd
fuzzer-dll : $(ZSTDDIR)/common/xxhash.c $(PRGDIR)/datagen.c fuzzer.c
	$(MAKE) -C $(ZSTDDIR) libzstd
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@$(EXT)

zbufftest : CPPFLAGS += -I$(ZSTDDIR)/deprecated
zbufftest : CFLAGS += -Wno-deprecated-declarations   # required to silence deprecation warnings
zbufftest : $(ZSTD_FILES) $(ZBUFF_FILES) $(PRGDIR)/datagen.c zbufftest.c
	$(CC) $(FLAGS) $^ -o $@$(EXT)

zbufftest32 : CPPFLAGS += -I$(ZSTDDIR)/deprecated
zbufftest32 : CFLAGS += -Wno-deprecated-declarations -m32
zbufftest32 : $(ZSTD_FILES) $(ZBUFF_FILES) $(PRGDIR)/datagen.c zbufftest.c
	$(CC) $(FLAGS) $^ -o $@$(EXT)

zbufftest-dll : CPPFLAGS += -I$(ZSTDDIR)/deprecated
zbufftest-dll : CFLAGS += -Wno-deprecated-declarations   # required to silence deprecation warnings
zbufftest-dll : LDFLAGS+= -L$(ZSTDDIR) -lzstd
zbufftest-dll : $(ZSTDDIR)/common/xxhash.c $(PRGDIR)/datagen.c zbufftest.c
	$(MAKE) -C $(ZSTDDIR) libzstd
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@$(EXT)

zstreamtest  : $(ZSTD_FILES) $(ZDICT_FILES) $(PRGDIR)/datagen.c zstreamtest.c
	$(CC)      $(FLAGS) $(MULTITHREAD) $^ -o $@$(EXT)

zstreamtest32 : $(ZSTD_FILES) $(ZDICT_FILES) $(PRGDIR)/datagen.c zstreamtest.c
	$(CC) -m32  $(FLAGS) $(MULTITHREAD) $^ -o $@$(EXT)

zstreamtest-dll : LDFLAGS+= -L$(ZSTDDIR) -lzstd
zstreamtest-dll : $(ZSTDDIR)/common/xxhash.c $(PRGDIR)/datagen.c zstreamtest.c
	$(MAKE) -C $(ZSTDDIR) libzstd
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@$(EXT)

paramgrill : $(ZSTD_FILES) $(PRGDIR)/datagen.c paramgrill.c
	$(CC)      $(FLAGS) $^ -lm -o $@$(EXT)

datagen : $(PRGDIR)/datagen.c datagencli.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

roundTripCrash : $(ZSTD_FILES) roundTripCrash.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

longmatch  : $(ZSTD_FILES) longmatch.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

invalidDictionaries  : $(ZSTD_FILES) invalidDictionaries.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

symbols  : symbols.c
	$(MAKE) -C $(ZSTDDIR) libzstd
ifneq (,$(filter Windows%,$(OS)))
	cp $(ZSTDDIR)/dll/libzstd.dll .
	$(CC) $(FLAGS) $^ -o $@$(EXT) -DZSTD_DLL_IMPORT=1 libzstd.dll
else
	$(CC) $(FLAGS) $^ -o $@$(EXT) -Wl,-rpath=$(ZSTDDIR) $(ZSTDDIR)/libzstd.so
endif

pool  : pool.c $(ZSTDDIR)/common/pool.c $(ZSTDDIR)/common/threading.c
	$(CC)    $(FLAGS) $(MULTITHREAD) $^ -o $@$(EXT)

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
        fullbench-lib$(EXT) fullbench-dll$(EXT) \
        fuzzer$(EXT) fuzzer32$(EXT) zbufftest$(EXT) zbufftest32$(EXT) \
        fuzzer-dll$(EXT) zstreamtest-dll$(EXT) zbufftest-dll$(EXT)\
        zstreamtest$(EXT) zstreamtest32$(EXT) \
        datagen$(EXT) paramgrill$(EXT) roundTripCrash$(EXT) longmatch$(EXT) \
        symbols$(EXT) invalidDictionaries$(EXT) pool$(EXT)
	@echo Cleaning completed


#----------------------------------------------------------------------------------
#make valgrindTest is validated only for Linux, OSX, BSD, Hurd and Solaris targets
#----------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS))
HOST_OS = POSIX

valgrindTest: VALGRIND = valgrind --leak-check=full --error-exitcode=1
valgrindTest: zstd datagen fuzzer fullbench
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

endif


ifneq (,$(filter MSYS%,$(shell uname)))
HOST_OS = MSYS
endif


#-----------------------------------------------------------------------------
#make tests validated only for MSYS, Linux, OSX, BSD, Hurd and Solaris targets
#-----------------------------------------------------------------------------
ifneq (,$(filter $(HOST_OS),MSYS POSIX))

DIFF:=diff
ifneq (,$(filter $(shell uname),SunOS))
DIFF:=gdiff
endif

zstd-playTests: datagen
	file $(ZSTD)
	ZSTD="$(QEMU_SYS) $(ZSTD)" ./playTests.sh $(ZSTDRTTEST)

test: test-zstd test-fullbench test-fuzzer test-zstream test-longmatch test-invalidDictionaries test-pool

test32: test-zstd32 test-fullbench32 test-fuzzer32 test-zstream32

test-all: test test32 valgrindTest

test-zstd: ZSTD = $(PRGDIR)/zstd
test-zstd: zstd zstd-playTests

test-zstd32: ZSTD = $(PRGDIR)/zstd32
test-zstd32: zstd32 zstd-playTests

test-zstd-nolegacy: ZSTD = $(PRGDIR)/zstd
test-zstd-nolegacy: zstd-nolegacy zstd-playTests

test-gzstd: gzstd
	$(PRGDIR)/zstd README.md test-zstd-speed.py
	gzip README.md test-zstd-speed.py
	cat README.md.zst test-zstd-speed.py.gz >zstd_gz.zst
	cat README.md.gz test-zstd-speed.py.zst >gz_zstd.gz
	$(PRGDIR)/zstd -d README.md.gz -o README2.md
	$(PRGDIR)/zstd -d README.md.gz test-zstd-speed.py.gz
	$(PRGDIR)/zstd -d zstd_gz.zst gz_zstd.gz
	$(DIFF) -q zstd_gz gz_zstd
	echo Hello World ZSTD | $(PRGDIR)/zstd -c - >hello.zst
	echo Hello World GZIP | gzip -c - >hello.gz
	echo Hello World TEXT >hello.txt
	cat hello.zst hello.gz hello.txt >hello_zst_gz_txt.gz
	$(PRGDIR)/zstd -dcf hello.*
	$(PRGDIR)/zstd -dcf - <hello_zst_gz_txt.gz

test-fullbench: fullbench datagen
	$(QEMU_SYS) ./fullbench -i1
	$(QEMU_SYS) ./fullbench -i1 -P0

test-fullbench32: fullbench32 datagen
	$(QEMU_SYS) ./fullbench32 -i1
	$(QEMU_SYS) ./fullbench32 -i1 -P0

test-fuzzer: fuzzer
	$(QEMU_SYS) ./fuzzer $(FUZZERTEST)

test-fuzzer32: fuzzer32
	$(QEMU_SYS) ./fuzzer32 $(FUZZERTEST)

test-zbuff: zbufftest
	$(QEMU_SYS) ./zbufftest $(ZSTREAM_TESTTIME)

test-zbuff32: zbufftest32
	$(QEMU_SYS) ./zbufftest32 $(ZSTREAM_TESTTIME)

test-zstream: zstreamtest
	$(QEMU_SYS) ./zstreamtest $(ZSTREAM_TESTTIME)

test-zstream32: zstreamtest32
	$(QEMU_SYS) ./zstreamtest32 $(ZSTREAM_TESTTIME)

test-longmatch: longmatch
	$(QEMU_SYS) ./longmatch

test-invalidDictionaries: invalidDictionaries
	$(QEMU_SYS) ./invalidDictionaries

test-symbols: symbols
	$(QEMU_SYS) ./symbols

test-pool: pool
	$(QEMU_SYS) ./pool

endif
