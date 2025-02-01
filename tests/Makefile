
# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
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

ZSTD_LEGACY_SUPPORT ?= 5
export ZSTD_LEGACY_SUPPORT

DEBUGLEVEL ?= 2
export DEBUGLEVEL  # transmit value to sub-makefiles

.PHONY: default
default: fullbench

LIBZSTD_MK_DIR := ../lib
include $(LIBZSTD_MK_DIR)/libzstd.mk

PRGDIR  = ../programs
PYTHON ?= python3
TESTARTEFACT := versionsTest

DEBUGFLAGS += -g -Wno-c++-compat
CPPFLAGS   += -I$(LIB_SRCDIR) -I$(LIB_SRCDIR)/common -I$(LIB_SRCDIR)/compress -I$(LIB_SRCDIR)/legacy \
              -I$(LIB_SRCDIR)/dictBuilder -I$(LIB_SRCDIR)/deprecated -I$(PRGDIR) \
              -DZSTD_WINDOW_OVERFLOW_CORRECT_FREQUENTLY=1

ZSTDCOMMON_FILES := $(sort $(ZSTD_COMMON_FILES))
ZSTDCOMP_FILES   := $(sort $(ZSTD_COMPRESS_FILES))
ZSTDDECOMP_FILES := $(sort $(ZSTD_DECOMPRESS_FILES))
ZSTDLEGACY_FILES := $(sort $(wildcard $(LIB_SRCDIR)/legacy/*.c))
ZSTD_FILES  := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES) $(ZSTDLEGACY_FILES)
ZDICT_FILES := $(sort $(ZSTD_DICTBUILDER_FILES))

ZSTD_F1 := $(sort $(wildcard $(ZSTD_FILES)))
ZSTD_OBJ1 := $(subst $(LIB_SRCDIR)/common/,zstdm_,$(ZSTD_F1))
ZSTD_OBJ2 := $(subst $(LIB_SRCDIR)/compress/,zstdc_,$(ZSTD_OBJ1))
ZSTD_OBJ3 := $(subst $(LIB_SRCDIR)/decompress/,zstdd_,$(ZSTD_OBJ2))
ZSTD_OBJ4 := $(subst $(LIB_SRCDIR)/legacy/,zstdl_,$(ZSTD_OBJ3))
ZSTD_OBJ5 := $(ZSTD_OBJ4:.c=.o)
ZSTD_OBJECTS := $(ZSTD_OBJ5:.S=.o)

ZSTDMT_OBJ1 := $(subst $(LIB_SRCDIR)/common/,zstdmt_m_,$(ZSTD_F1))
ZSTDMT_OBJ2 := $(subst $(LIB_SRCDIR)/compress/,zstdmt_c_,$(ZSTDMT_OBJ1))
ZSTDMT_OBJ3 := $(subst $(LIB_SRCDIR)/decompress/,zstdmt_d_,$(ZSTDMT_OBJ2))
ZSTDMT_OBJ4 := $(subst $(LIB_SRCDIR)/legacy/,zstdmt_l_,$(ZSTDMT_OBJ3))
ZSTDMT_OBJ5 := $(ZSTDMT_OBJ4:.c=.o)
ZSTDMT_OBJECTS := $(ZSTDMT_OBJ5:.S=.o)

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

.PHONY: all
all: fullbench fuzzer zstreamtest paramgrill datagen decodecorpus roundTripCrash poolTests

.PHONY: all32
all32: fullbench32 fuzzer32 zstreamtest32

.PHONY: allnothread
allnothread: MULTITHREAD_CPP=
allnothread: MULTITHREAD_LD=
allnothread: fullbench fuzzer paramgrill datagen decodecorpus

# note : broken : requires symbols unavailable from dynamic library
.PHONY: dll
dll: fuzzer-dll zstreamtest-dll

.PHONY: zstd zstd32 zstd-nolegacy  # only external makefile knows how to build or update them
zstd zstd32 zstd-nolegacy zstd-dll:
	$(MAKE) -C $(PRGDIR) $@ MOREFLAGS+="$(DEBUGFLAGS)"

.PHONY: libzstd
libzstd :
	$(MAKE) -C $(LIB_SRCDIR) libzstd MOREFLAGS+="$(DEBUGFLAGS)"

%-dll : libzstd
%-dll : LDFLAGS += -L$(LIB_BINDIR) -lzstd

$(LIB_BINDIR)/libzstd.a :
	$(MAKE) -C $(LIB_SRCDIR) libzstd.a

zstdm_%.o : $(LIB_SRCDIR)/common/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

zstdc_%.o : $(LIB_SRCDIR)/compress/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

zstdd_%.o : $(LIB_SRCDIR)/decompress/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

zstdd_%.o : $(LIB_SRCDIR)/decompress/%.S
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) $< -o $@

zstdl_%.o : $(LIB_SRCDIR)/legacy/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

zstdmt%.o : CPPFLAGS += $(MULTITHREAD_CPP)

zstdmt_m_%.o : $(LIB_SRCDIR)/common/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

zstdmt_c_%.o : $(LIB_SRCDIR)/compress/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

zstdmt_d_%.o : $(LIB_SRCDIR)/decompress/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

zstdmt_d_%.o : $(LIB_SRCDIR)/decompress/%.S
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) $< -o $@

zstdmt_l_%.o : $(LIB_SRCDIR)/legacy/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

FULLBENCHS := fullbench fullbench32
CLEAN += $(FULLBENCHS)
fullbench32: CPPFLAGS += -m32
$(FULLBENCHS) : CPPFLAGS += $(MULTITHREAD_CPP) -Wno-deprecated-declarations
$(FULLBENCHS) : LDFLAGS += $(MULTITHREAD_LD)
$(FULLBENCHS) : DEBUGFLAGS = -DNDEBUG  # turn off assert() for speed measurements
$(FULLBENCHS) : DEBUGLEVEL = 0  # turn off assert() for speed measurements
$(FULLBENCHS) : $(ZSTD_FILES)
$(FULLBENCHS) : $(PRGDIR)/datagen.c $(PRGDIR)/lorem.c $(PRGDIR)/util.c $(PRGDIR)/timefn.c $(PRGDIR)/benchfn.c fullbench.c
	$(LINK.c) $^ -o $@$(EXT)

CLEAN += fullbench-lib
fullbench-lib : CPPFLAGS += -DXXH_NAMESPACE=ZSTD_
fullbench-lib : $(PRGDIR)/datagen.c $(PRGDIR)/lorem.c $(PRGDIR)/util.c $(PRGDIR)/timefn.c $(PRGDIR)/benchfn.c $(LIB_SRCDIR)/libzstd.a fullbench.c
	$(LINK.c) $^ -o $@$(EXT)

# note : broken : requires symbols unavailable from dynamic library
fullbench-dll: $(PRGDIR)/datagen.c $(PRGDIR)/util.c $(PRGDIR)/benchfn.c $(PRGDIR)/timefn.c fullbench.c
#	$(CC) $(FLAGS) $(filter %.c,$^) -o $@$(EXT) -DZSTD_DLL_IMPORT=1 $(LIB_SRCDIR)/dll/libzstd.dll
	$(LINK.c) $^ $(LDLIBS) -o $@$(EXT)

CLEAN += fuzzer fuzzer32
fuzzer : CPPFLAGS += $(MULTITHREAD_CPP) -Wno-deprecated-declarations
fuzzer : LDFLAGS += $(MULTITHREAD_LD)
fuzzer : $(ZSTDMT_OBJECTS)
fuzzer fuzzer32 : $(ZDICT_FILES) $(PRGDIR)/util.c $(PRGDIR)/timefn.c $(PRGDIR)/datagen.c fuzzer.c

fuzzer32 : CFLAGS += -m32 $(MULTITHREAD)
fuzzer32 : $(ZSTD_FILES)
	$(LINK.c) $^ -o $@$(EXT)

# note : broken : requires symbols unavailable from dynamic library
fuzzer-dll : $(LIB_SRCDIR)/common/xxhash.c $(PRGDIR)/util.c $(PRGDIR)/timefn.c $(PRGDIR)/datagen.c fuzzer.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) -o $@$(EXT)

CLEAN += zstreamtest zstreamtest32
ZSTREAM_LOCAL_FILES := $(PRGDIR)/datagen.c $(PRGDIR)/util.c $(PRGDIR)/timefn.c seqgen.c zstreamtest.c external_matchfinder.c
ZSTREAM_PROPER_FILES := $(ZDICT_FILES) $(ZSTREAM_LOCAL_FILES)
ZSTREAMFILES := $(ZSTD_FILES) $(ZSTREAM_PROPER_FILES)
zstreamtest32 : CFLAGS += -m32
zstreamtest zstreamtest32 : CPPFLAGS += $(MULTITHREAD_CPP)
zstreamtest zstreamtest32 : LDFLAGS += $(MULTITHREAD_LD)
zstreamtest : $(ZSTDMT_OBJECTS) $(ZSTREAM_PROPER_FILES)
zstreamtest32 : $(ZSTREAMFILES)
zstreamtest zstreamtest32 :
	$(LINK.c) $^ -o $@$(EXT)

CLEAN += zstreamtest_asan
zstreamtest_asan : CFLAGS += -fsanitize=address
zstreamtest_asan : $(ZSTREAMFILES)
	$(LINK.c) $(MULTITHREAD) $^ -o $@$(EXT)

CLEAN += zstreamtest_tsan
zstreamtest_tsan : CFLAGS += -fsanitize=thread
zstreamtest_tsan : $(ZSTREAMFILES)
	$(LINK.c) $(MULTITHREAD) $^ -o $@$(EXT)

CLEAN += zstreamtest_ubsan
zstreamtest_ubsan : CFLAGS += -fsanitize=undefined
zstreamtest_ubsan : $(ZSTREAMFILES)
	$(LINK.c) $(MULTITHREAD) $^ -o $@$(EXT)

# note : broken : requires symbols unavailable from dynamic library
zstreamtest-dll : $(LIB_SRCDIR)/common/xxhash.c  # xxh symbols not exposed from dll
zstreamtest-dll : $(ZSTREAM_LOCAL_FILES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) -o $@$(EXT)

CLEAN += paramgrill
paramgrill : DEBUGFLAGS =   # turn off debug for speed measurements
paramgrill : LDLIBS += -lm
paramgrill : $(ZSTD_FILES) $(PRGDIR)/util.c $(PRGDIR)/timefn.c $(PRGDIR)/benchfn.c $(PRGDIR)/benchzstd.c $(PRGDIR)/datagen.c $(PRGDIR)/lorem.c paramgrill.c

CLEAN += datagen
datagen : $(PRGDIR)/datagen.c $(PRGDIR)/lorem.c loremOut.c datagencli.c
	$(LINK.c) $^ -o $@$(EXT)

CLEAN += roundTripCrash
roundTripCrash: CFLAGS += $(MULTITHREAD)
roundTripCrash : $(ZSTD_OBJECTS) roundTripCrash.c

CLEAN += longmatch
longmatch : $(ZSTD_OBJECTS) longmatch.c

CLEAN += largeDictionary
largeDictionary: CFLAGS += $(MULTITHREAD)
largeDictionary: $(ZSTDMT_OBJECTS) $(PRGDIR)/datagen.c largeDictionary.c

CLEAN += invalidDictionaries
invalidDictionaries : $(ZSTD_OBJECTS) invalidDictionaries.c

CLEAN += legacy
legacy : CPPFLAGS += -UZSTD_LEGACY_SUPPORT -DZSTD_LEGACY_SUPPORT=4
legacy : $(ZSTD_FILES) legacy.c

CLEAN += decodecorpus
decodecorpus : LDLIBS += -lm
decodecorpus : $(filter-out zstdc_zstd_compress.o, $(ZSTD_OBJECTS)) $(ZDICT_FILES) $(PRGDIR)/util.c $(PRGDIR)/timefn.c decodecorpus.c

CLEAN += poolTests
poolTests : $(PRGDIR)/util.c $(PRGDIR)/timefn.c poolTests.c $(LIB_SRCDIR)/common/pool.c $(LIB_SRCDIR)/common/threading.c $(LIB_SRCDIR)/common/zstd_common.c $(LIB_SRCDIR)/common/error_private.c
	$(LINK.c) $(MULTITHREAD) $^ -o $@$(EXT)

.PHONY: versionsTest
versionsTest: clean
	$(PYTHON) test-zstd-versions.py

.PHONY: automated_benchmarking
automated_benchmarking: clean
	$(PYTHON) automated_benchmarking.py

# make checkTag : check that release tag corresponds to release version
CLEAN += checkTag
checkTag.o : $(LIB_SRCDIR)/zstd.h

.PHONY: clean
clean:
	$(MAKE) -C $(LIB_SRCDIR) clean
	$(MAKE) -C $(PRGDIR) clean
	$(MAKE) -C fuzz clean
	$(RM) -R $(TESTARTEFACT)
	$(RM) -r tmp*  # some test directories are named tmp*
	$(RM) $(CLEAN) core *.o *.tmp result* *.gcda dictionary *.zst \
        $(PRGDIR)/zstd$(EXT) $(PRGDIR)/zstd32$(EXT) \
        fullbench-dll$(EXT) fuzzer-dll$(EXT) zstreamtest-dll$(EXT)
	@echo Cleaning completed


#----------------------------------------------------------------------------------
# valgrind tests validated only for some posix platforms
#----------------------------------------------------------------------------------
UNAME := $(shell sh -c 'MSYSTEM="MSYS" uname')
ifneq (,$(filter Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS AIX CYGWIN_NT%,$(UNAME)))
HOST_OS = POSIX

.PHONY: test-valgrind
test-valgrind: VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1
test-valgrind: zstd datagen fuzzer fullbench
	@echo "\n ---- valgrind tests : memory analyzer ----"
	$(VALGRIND) ./datagen -g50M > $(VOID)
	$(VALGRIND) $(PRGDIR)/zstd ; if [ $$? -eq 0 ] ; then echo "zstd without argument should have failed"; false; fi
	./datagen -g80 | $(VALGRIND) $(PRGDIR)/zstd - -c > $(VOID)
	./datagen -g16KB | $(VALGRIND) $(PRGDIR)/zstd -vf - -c > $(VOID)
	./datagen -g2930KB | $(VALGRIND) $(PRGDIR)/zstd -5 -vf - -o tmp
	$(VALGRIND) $(PRGDIR)/zstd -vdf tmp -c > $(VOID)
	./datagen -g64MB | $(VALGRIND) $(PRGDIR)/zstd -vf - -c > $(VOID)
	$(RM) tmp
	$(VALGRIND) ./fuzzer -T1mn -t1
	$(VALGRIND) ./fullbench -i1

endif

ifneq (,$(filter MINGW% MSYS%,$(UNAME)))
  HOST_OS = MSYS
endif


#-----------------------------------------------------------------------------
# make tests validated only for below targets
#-----------------------------------------------------------------------------
ifneq (,$(filter MSYS POSIX,$(HOST_OS)))

DIFF:=diff
ifneq (,$(filter SunOS,$(UNAME)))
  DIFF:=gdiff
endif

.PHONY: list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$' | xargs

.PHONY: check
check: ZSTDRTTEST=  # remove long tests
check: test-zstd
	@echo "\n******************************"
	@echo "All tests completed successfully"
	@echo "******************************"

.PHONY: fuzztest
fuzztest: test-fuzzer test-zstream test-decodecorpus

.PHONY: test
test: test-zstd test-cli-tests test-fullbench test-fuzzer test-zstream test-invalidDictionaries test-legacy test-decodecorpus
ifeq ($(QEMU_SYS),)
test: test-pool
endif
	@echo "\n******************************"
	@echo "All tests completed successfully"
	@echo "******************************"

.PHONY: test32
test32: test-zstd32 test-fullbench32 test-fuzzer32 test-zstream32

.PHONY: test-all
test-all: test test32 test-decodecorpus-cli

.PHONY: test-zstd test-zstd32 test-zstd-nolegacy
test-zstd: ZSTD = $(PRGDIR)/zstd
test-zstd: zstd

.PHONY: test-zstd-dll
test-zstd-dll: ZSTD = $(PRGDIR)/zstd
test-zstd-dll: zstd-dll

test-zstd32: ZSTD = $(PRGDIR)/zstd32
test-zstd32: zstd32

test-zstd-nolegacy: ZSTD = $(PRGDIR)/zstd-nolegacy
test-zstd-nolegacy: zstd-nolegacy

test-zstd test-zstd32 test-zstd-nolegacy test-zstd-dll: datagen
	file $(ZSTD)
	EXE_PREFIX="$(QEMU_SYS)" ZSTD_BIN="$(ZSTD)" DATAGEN_BIN=./datagen ./playTests.sh $(ZSTDRTTEST)

.PHONY: test-cli-tests
test-cli-tests: ZSTD = $(PRGDIR)/zstd
test-cli-tests: zstd datagen
	file $(ZSTD)
	./cli-tests/run.py --exec-prefix="$(QEMU_SYS)" --zstd="$(ZSTD)" --datagen=./datagen

.PHONY: test-fullbench
test-fullbench: fullbench datagen
	$(QEMU_SYS) ./fullbench -i1
	$(QEMU_SYS) ./fullbench -i1 -P0

.PHONY: test-fullbench32
test-fullbench32: fullbench32 datagen
	$(QEMU_SYS) ./fullbench32 -i1
	$(QEMU_SYS) ./fullbench32 -i1 -P0

.PHONY: test-fuzzer
test-fuzzer: fuzzer
	$(QEMU_SYS) ./fuzzer -v $(FUZZERTEST) $(FUZZER_FLAGS)

# Note : this test presumes `fuzzer` will be built
.PHONY: test-fuzzer-stackmode
test-fuzzer-stackmode: MOREFLAGS += -DZSTD_HEAPMODE=0
test-fuzzer-stackmode: test-fuzzer

.PHONY: test-fuzzer32
test-fuzzer32: fuzzer32
	$(QEMU_SYS) ./fuzzer32 -v $(FUZZERTEST) $(FUZZER_FLAGS)

.PHONY: test-zstream
test-zstream: zstreamtest
	$(QEMU_SYS) ./zstreamtest -v $(ZSTREAM_TESTTIME) $(FUZZER_FLAGS)
	$(QEMU_SYS) ./zstreamtest --newapi -t1 $(ZSTREAM_TESTTIME) $(FUZZER_FLAGS)

test-zstream32: zstreamtest32
	$(QEMU_SYS) ./zstreamtest32 -v $(ZSTREAM_TESTTIME) $(FUZZER_FLAGS)

test-longmatch: longmatch
	$(QEMU_SYS) ./longmatch

test-largeDictionary: largeDictionary
	$(QEMU_SYS) ./largeDictionary

test-invalidDictionaries: invalidDictionaries
	$(QEMU_SYS) ./invalidDictionaries

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
test-lz4: zstd decodecorpus datagen
	[ -f lz4 ] || ln -s $(PRGDIR)/zstd lz4
	[ -f unlz4 ] || ln -s $(PRGDIR)/zstd unlz4

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

	./datagen -g384KB | $(ZSTD) --format=lz4 | $(ZSTD) -d > /dev/null

	rm tmp lz4 unlz4

endif
