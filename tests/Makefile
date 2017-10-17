# ################################################################
# Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################
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

DEBUGLEVEL ?= 1
DEBUGFLAGS  = -g -DZSTD_DEBUG=$(DEBUGLEVEL)
CPPFLAGS   += -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/compress \
              -I$(ZSTDDIR)/dictBuilder -I$(ZSTDDIR)/deprecated -I$(PRGDIR)
CFLAGS     ?= -O3
CFLAGS     += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow                 \
              -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
              -Wstrict-prototypes -Wundef -Wformat-security                   \
              -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings      \
              -Wredundant-decls
CFLAGS     += $(DEBUGFLAGS) $(MOREFLAGS)
FLAGS       = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)


ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES   := $(ZSTDDIR)/compress/*.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/*.c
ZSTD_FILES  := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)
ZBUFF_FILES := $(ZSTDDIR)/deprecated/*.c
ZDICT_FILES := $(ZSTDDIR)/dictBuilder/*.c


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
MULTITHREAD_CPP = -DZSTD_MULTITHREAD
MULTITHREAD_LD  =
else
EXT =
MULTITHREAD_CPP = -DZSTD_MULTITHREAD
MULTITHREAD_LD  = -pthread
endif
MULTITHREAD = $(MULTITHREAD_CPP) $(MULTITHREAD_LD)

VOID = /dev/null
ZSTREAM_TESTTIME ?= -T90s
FUZZERTEST ?= -T200s
ZSTDRTTEST = --test-large-data
DECODECORPUS_TESTTIME ?= -T30

.PHONY: default all all32 allnothread dll clean test test32 test-all namespaceTest versionsTest

default: fullbench

all: fullbench fuzzer zstreamtest paramgrill datagen decodecorpus

all32: fullbench32 fuzzer32 zstreamtest32

allnothread: fullbench fuzzer paramgrill datagen decodecorpus

dll: fuzzer-dll zstreamtest-dll

zstd:
	$(MAKE) -C $(PRGDIR) $@

zstd32:
	$(MAKE) -C $(PRGDIR) $@

zstd-nolegacy:
	$(MAKE) -C $(PRGDIR) $@

gzstd:
	$(MAKE) -C $(PRGDIR) zstd HAVE_ZLIB=1

fullbench32: CPPFLAGS += -m32
fullbench fullbench32 : CPPFLAGS += $(MULTITHREAD_CPP)
fullbench fullbench32 : LDFLAGS += $(MULTITHREAD_LD)
fullbench fullbench32 : DEBUGFLAGS =   # turn off assert() for speed measurements
fullbench fullbench32 : $(ZSTD_FILES) $(PRGDIR)/datagen.c fullbench.c
	$(CC) $(FLAGS) $^ -o $@$(EXT)

fullbench-lib: $(PRGDIR)/datagen.c fullbench.c
	$(MAKE) -C $(ZSTDDIR) libzstd.a
	$(CC) $(FLAGS) $^ -o $@$(EXT) $(ZSTDDIR)/libzstd.a

fullbench-dll: $(PRGDIR)/datagen.c fullbench.c
	$(MAKE) -C $(ZSTDDIR) libzstd
	$(CC) $(FLAGS) $^ -o $@$(EXT) -DZSTD_DLL_IMPORT=1 $(ZSTDDIR)/dll/libzstd.dll

fuzzer : CPPFLAGS += $(MULTITHREAD_CPP)
fuzzer : LDFLAGS += $(MULTITHREAD_LD)
fuzzer32: CFLAGS += -m32
fuzzer fuzzer32 : $(ZSTD_FILES) $(ZDICT_FILES) $(PRGDIR)/datagen.c fuzzer.c
	$(CC) $(FLAGS) $^ -o $@$(EXT)

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

ZSTREAMFILES := $(ZSTD_FILES) $(ZDICT_FILES) $(PRGDIR)/datagen.c seqgen.c zstreamtest.c
zstreamtest : CPPFLAGS += $(MULTITHREAD_CPP)
zstreamtest : LDFLAGS += $(MULTITHREAD_LD)
zstreamtest : $(ZSTREAMFILES)
	$(CC) $(FLAGS) $^ -o $@$(EXT)

zstreamtest32 : CFLAGS += -m32
zstreamtest32 : $(ZSTREAMFILES)
	$(CC) $(FLAGS) $(MULTITHREAD) $^ -o $@$(EXT)

zstreamtest_asan : CFLAGS += -fsanitize=address
zstreamtest_asan : $(ZSTREAMFILES)
	$(CC) $(FLAGS) $(MULTITHREAD) $^ -o $@$(EXT)

zstreamtest_tsan : CFLAGS += -fsanitize=thread
zstreamtest_tsan : $(ZSTREAMFILES)
	$(CC) $(FLAGS) $(MULTITHREAD) $^ -o $@$(EXT)

zstreamtest-dll : LDFLAGS+= -L$(ZSTDDIR) -lzstd
zstreamtest-dll : $(ZSTDDIR)/common/xxhash.c $(PRGDIR)/datagen.c zstreamtest.c
	$(MAKE) -C $(ZSTDDIR) libzstd
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@$(EXT)

paramgrill : DEBUGFLAGS =
paramgrill : $(ZSTD_FILES) $(PRGDIR)/datagen.c paramgrill.c
	$(CC)      $(FLAGS) $^ -lm -o $@$(EXT)

datagen : $(PRGDIR)/datagen.c datagencli.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

roundTripCrash : $(ZSTD_FILES) roundTripCrash.c
	$(CC)      $(FLAGS) $(MULTITHREAD) $^ -o $@$(EXT)

longmatch  : $(ZSTD_FILES) longmatch.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

invalidDictionaries  : $(ZSTD_FILES) invalidDictionaries.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

legacy : CFLAGS+= -DZSTD_LEGACY_SUPPORT=4
legacy : CPPFLAGS+= -I$(ZSTDDIR)/legacy
legacy : $(ZSTD_FILES) $(wildcard $(ZSTDDIR)/legacy/*.c) legacy.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

decodecorpus	: $(filter-out $(ZSTDDIR)/compress/zstd_compress.c, $(wildcard $(ZSTD_FILES))) $(ZDICT_FILES) decodecorpus.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT) -lm

symbols  : symbols.c
	$(MAKE) -C $(ZSTDDIR) libzstd
ifneq (,$(filter Windows%,$(OS)))
	cp $(ZSTDDIR)/dll/libzstd.dll .
	$(CC) $(FLAGS) $^ -o $@$(EXT) -DZSTD_DLL_IMPORT=1 libzstd.dll
else
	$(CC) $(FLAGS) $^ -o $@$(EXT) -Wl,-rpath=$(ZSTDDIR) $(ZSTDDIR)/libzstd.so
endif

poolTests : poolTests.c $(ZSTDDIR)/common/pool.c $(ZSTDDIR)/common/threading.c $(ZSTDDIR)/common/zstd_common.c $(ZSTDDIR)/common/error_private.c
	$(CC) $(FLAGS) $(MULTITHREAD) $^ -o $@$(EXT)

namespaceTest:
	if $(CC) namespaceTest.c ../lib/common/xxhash.c -o $@ ; then echo compilation should fail; exit 1 ; fi
	$(RM) $@

versionsTest: clean
	$(PYTHON) test-zstd-versions.py

clean:
	$(MAKE) -C $(ZSTDDIR) clean
	@$(RM) -fR $(TESTARTEFACT)
	@$(RM) -f core *.o tmp* result* *.gcda dictionary *.zst \
        $(PRGDIR)/zstd$(EXT) $(PRGDIR)/zstd32$(EXT) \
        fullbench$(EXT) fullbench32$(EXT) \
        fullbench-lib$(EXT) fullbench-dll$(EXT) \
        fuzzer$(EXT) fuzzer32$(EXT) zbufftest$(EXT) zbufftest32$(EXT) \
        fuzzer-dll$(EXT) zstreamtest-dll$(EXT) zbufftest-dll$(EXT)\
        zstreamtest$(EXT) zstreamtest32$(EXT) \
        datagen$(EXT) paramgrill$(EXT) roundTripCrash$(EXT) longmatch$(EXT) \
        symbols$(EXT) invalidDictionaries$(EXT) legacy$(EXT) poolTests$(EXT) \
        decodecorpus$(EXT)
	@echo Cleaning completed


#----------------------------------------------------------------------------------
#make valgrindTest is validated only for Linux, OSX, BSD, Hurd and Solaris targets
#----------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS))
HOST_OS = POSIX

valgrindTest: VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1
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
# make tests validated only for below targets
#-----------------------------------------------------------------------------
ifneq (,$(filter $(HOST_OS),MSYS POSIX))

DIFF:=diff
ifneq (,$(filter $(shell uname),SunOS))
DIFF:=gdiff
endif

zstd-playTests: datagen
	file $(ZSTD)
	ZSTD="$(QEMU_SYS) $(ZSTD)" ./playTests.sh $(ZSTDRTTEST)

shortest: ZSTDRTTEST=
shortest: test-zstd

fuzztest: test-fuzzer test-zstream test-decodecorpus

test: test-zstd test-fullbench test-fuzzer test-zstream test-invalidDictionaries test-legacy test-decodecorpus
ifeq ($(QEMU_SYS),)
test: test-pool
endif

test32: test-zstd32 test-fullbench32 test-fuzzer32 test-zstream32

test-all: test test32 valgrindTest test-decodecorpus-cli

test-zstd: ZSTD = $(PRGDIR)/zstd
test-zstd: zstd zstd-playTests

test-zstd32: ZSTD = $(PRGDIR)/zstd32
test-zstd32: zstd32 zstd-playTests

test-zstd-nolegacy: ZSTD = $(PRGDIR)/zstd-nolegacy
test-zstd-nolegacy: zstd-nolegacy zstd-playTests

test-gzstd: gzstd
	$(PRGDIR)/zstd -f README.md test-zstd-speed.py
	gzip -f README.md test-zstd-speed.py
	cat README.md.zst test-zstd-speed.py.gz >zstd_gz.zst
	cat README.md.gz test-zstd-speed.py.zst >gz_zstd.gz
	$(PRGDIR)/zstd -df README.md.gz -o README2.md
	$(PRGDIR)/zstd -df README.md.gz test-zstd-speed.py.gz
	$(PRGDIR)/zstd -df zstd_gz.zst gz_zstd.gz
	$(DIFF) -q zstd_gz gz_zstd
	echo Hello World ZSTD | $(PRGDIR)/zstd -c - >hello.zst
	echo Hello World GZIP | gzip -c - >hello.gz
	echo Hello World TEXT >hello.txt
	cat hello.zst hello.gz hello.txt >hello_zst_gz_txt.gz
	$(PRGDIR)/zstd -dcf hello.*
	$(PRGDIR)/zstd -dcf - <hello_zst_gz_txt.gz
	$(RM) *.gz *.zst README2.md gz_zstd zstd_gz hello.txt

test-fullbench: fullbench datagen
	$(QEMU_SYS) ./fullbench -i1
	$(QEMU_SYS) ./fullbench -i1 -P0

test-fullbench32: fullbench32 datagen
	$(QEMU_SYS) ./fullbench32 -i1
	$(QEMU_SYS) ./fullbench32 -i1 -P0

test-fuzzer: fuzzer
	$(QEMU_SYS) ./fuzzer $(FUZZERTEST) $(FUZZER_FLAGS)

test-fuzzer32: fuzzer32
	$(QEMU_SYS) ./fuzzer32 $(FUZZERTEST) $(FUZZER_FLAGS)

test-zbuff: zbufftest
	$(QEMU_SYS) ./zbufftest $(ZSTREAM_TESTTIME)

test-zbuff32: zbufftest32
	$(QEMU_SYS) ./zbufftest32 $(ZSTREAM_TESTTIME)

test-zstream: zstreamtest
	$(QEMU_SYS) ./zstreamtest $(ZSTREAM_TESTTIME) $(FUZZER_FLAGS)
	$(QEMU_SYS) ./zstreamtest --mt -t1 $(ZSTREAM_TESTTIME) $(FUZZER_FLAGS)
	$(QEMU_SYS) ./zstreamtest --newapi -t1 $(ZSTREAM_TESTTIME) $(FUZZER_FLAGS)
	$(QEMU_SYS) ./zstreamtest --opaqueapi -t1 $(ZSTREAM_TESTTIME) $(FUZZER_FLAGS)

test-zstream32: zstreamtest32
	$(QEMU_SYS) ./zstreamtest32 $(ZSTREAM_TESTTIME) $(FUZZER_FLAGS)

test-longmatch: longmatch
	$(QEMU_SYS) ./longmatch

test-invalidDictionaries: invalidDictionaries
	$(QEMU_SYS) ./invalidDictionaries

test-symbols: symbols
	$(QEMU_SYS) ./symbols

test-legacy: legacy
	$(QEMU_SYS) ./legacy

test-decodecorpus: decodecorpus
	$(QEMU_SYS) ./decodecorpus -t $(DECODECORPUS_TESTTIME)

test-decodecorpus-cli: decodecorpus
	@echo "\n ---- decodecorpus basic cli tests ----"
	@mkdir testdir
	./decodecorpus -n5 -otestdir -ptestdir
	@cd testdir && \
	$(ZSTD) -d z000000.zst -o tmp0 && \
	$(ZSTD) -d z000001.zst -o tmp1 && \
	$(ZSTD) -d z000002.zst -o tmp2 && \
	$(ZSTD) -d z000003.zst -o tmp3 && \
	$(ZSTD) -d z000004.zst -o tmp4 && \
	diff z000000 tmp0 && \
	diff z000001 tmp1 && \
	diff z000002 tmp2 && \
	diff z000003 tmp3 && \
	diff z000004 tmp4 && \
	rm ./* && \
	cd ..
	@echo "\n ---- decodecorpus dictionary cli tests ----"
	./decodecorpus -n5 -otestdir -ptestdir --use-dict=1MB
	@cd testdir && \
	$(ZSTD) -d z000000.zst -D dictionary -o tmp0 && \
	$(ZSTD) -d z000001.zst -D dictionary -o tmp1 && \
	$(ZSTD) -d z000002.zst -D dictionary -o tmp2 && \
	$(ZSTD) -d z000003.zst -D dictionary -o tmp3 && \
	$(ZSTD) -d z000004.zst -D dictionary -o tmp4 && \
	diff z000000 tmp0 && \
	diff z000001 tmp1 && \
	diff z000002 tmp2 && \
	diff z000003 tmp3 && \
	diff z000004 tmp4 && \
	cd ..
	@rm -rf testdir

test-pool: poolTests
	$(QEMU_SYS) ./poolTests

test-lz4: ZSTD = LD_LIBRARY_PATH=/usr/local/lib $(PRGDIR)/zstd
test-lz4: ZSTD_LZ4 = LD_LIBRARY_PATH=/usr/local/lib ./lz4
test-lz4: ZSTD_UNLZ4 = LD_LIBRARY_PATH=/usr/local/lib ./unlz4
test-lz4: zstd decodecorpus
	ln -s $(PRGDIR)/zstd lz4
	ln -s $(PRGDIR)/zstd unlz4

	./decodecorpus -ptmp
	# lz4 -> zstd
	lz4 < tmp | \
	$(ZSTD) -d | \
	cmp - tmp
	lz4 < tmp | \
	$(ZSTD_UNLZ4) | \
	cmp - tmp
	# zstd -> lz4
	$(ZSTD) --format=lz4 < tmp | \
	lz4 -d | \
	cmp - tmp
	$(ZSTD_LZ4) < tmp | \
	lz4 -d | \
	cmp - tmp
	# zstd -> zstd
	$(ZSTD) --format=lz4 < tmp | \
	$(ZSTD) -d | \
	cmp - tmp
	# zstd -> zstd
	$(ZSTD) < tmp | \
	$(ZSTD) -d | \
	cmp - tmp

	rm tmp lz4 unlz4

endif
