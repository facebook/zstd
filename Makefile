# ################################################################
# Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

PRGDIR   = programs
ZSTDDIR  = lib
BUILDIR  = build
ZWRAPDIR = zlibWrapper
TESTDIR  = tests
FUZZDIR  = $(TESTDIR)/fuzz

# Define nul output
VOID = /dev/null

ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif

.PHONY: default
default: lib-release zstd-release

.PHONY: all
all: | allmost examples manual contrib

.PHONY: allmost
allmost: allzstd
	$(MAKE) -C $(ZWRAPDIR) all

#skip zwrapper, can't build that on alternate architectures without the proper zlib installed
.PHONY: allzstd
allzstd:
	$(MAKE) -C $(ZSTDDIR) all
	$(MAKE) -C $(PRGDIR) all
	$(MAKE) -C $(TESTDIR) all

.PHONY: all32
all32:
	$(MAKE) -C $(PRGDIR) zstd32
	$(MAKE) -C $(TESTDIR) all32

.PHONY: lib
lib:
	@$(MAKE) -C $(ZSTDDIR) $@

.PHONY: lib-release
lib-release:
	@$(MAKE) -C $(ZSTDDIR)

.PHONY: zstd
zstd:
	@$(MAKE) -C $(PRGDIR) $@
	cp $(PRGDIR)/zstd$(EXT) .

.PHONY: zstd-release
zstd-release:
	@$(MAKE) -C $(PRGDIR)
	cp $(PRGDIR)/zstd$(EXT) .

.PHONY: zstdmt
zstdmt:
	@$(MAKE) -C $(PRGDIR) $@
	cp $(PRGDIR)/zstd$(EXT) ./zstdmt$(EXT)

.PHONY: zlibwrapper
zlibwrapper:
	$(MAKE) -C $(ZWRAPDIR) test

.PHONY: test
test:
	$(MAKE) -C $(PRGDIR) allVariants MOREFLAGS+="-g -DZSTD_DEBUG=1"
	$(MAKE) -C $(TESTDIR) $@

.PHONY: shortest
shortest:
	$(MAKE) -C $(TESTDIR) $@

.PHONY: check
check: shortest

.PHONY: examples
examples:
	CPPFLAGS=-I../lib LDFLAGS=-L../lib $(MAKE) -C examples/ all

.PHONY: manual
manual:
	$(MAKE) -C contrib/gen_html $@

.PHONY: contrib
contrib: lib
	$(MAKE) -C contrib/pzstd all
	$(MAKE) -C contrib/seekable_format/examples all
	$(MAKE) -C contrib/adaptive-compression all

.PHONY: cleanTabs
cleanTabs:
	cd contrib; ./cleanTabs

.PHONY: clean
clean:
	@$(MAKE) -C $(ZSTDDIR) $@ > $(VOID)
	@$(MAKE) -C $(PRGDIR) $@ > $(VOID)
	@$(MAKE) -C $(TESTDIR) $@ > $(VOID)
	@$(MAKE) -C $(ZWRAPDIR) $@ > $(VOID)
	@$(MAKE) -C examples/ $@ > $(VOID)
	@$(MAKE) -C contrib/gen_html $@ > $(VOID)
	@$(MAKE) -C contrib/pzstd $@ > $(VOID)
	@$(MAKE) -C contrib/seekable_format/examples $@ > $(VOID)
	@$(MAKE) -C contrib/adaptive-compression $@ > $(VOID)
	@$(RM) zstd$(EXT) zstdmt$(EXT) tmp*
	@$(RM) -r lz4
	@echo Cleaning completed

#------------------------------------------------------------------------------
# make install is validated only for Linux, OSX, Hurd and some BSD targets
#------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU FreeBSD DragonFly NetBSD MSYS_NT))

HOST_OS = POSIX
CMAKE_PARAMS = -DZSTD_BUILD_CONTRIB:BOOL=ON -DZSTD_BUILD_STATIC:BOOL=ON -DZSTD_BUILD_TESTS:BOOL=ON -DZSTD_ZLIB_SUPPORT:BOOL=ON -DZSTD_LZMA_SUPPORT:BOOL=ON

.PHONY: list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$' | xargs

.PHONY: install clangtest armtest usan asan uasan
install:
	@$(MAKE) -C $(ZSTDDIR) $@
	@$(MAKE) -C $(PRGDIR) $@

.PHONY: uninstall
uninstall:
	@$(MAKE) -C $(ZSTDDIR) $@
	@$(MAKE) -C $(PRGDIR) $@

.PHONY: travis-install
travis-install:
	$(MAKE) install PREFIX=~/install_test_dir

.PHONY: gcc5build
gcc5build: clean
	gcc-5 -v
	CC=gcc-5 $(MAKE) all MOREFLAGS="-Werror"

.PHONY: gcc6build
gcc6build: clean
	gcc-6 -v
	CC=gcc-6 $(MAKE) all MOREFLAGS="-Werror"

.PHONY: gcc7build
gcc7build: clean
	gcc-7 -v
	CC=gcc-7 $(MAKE) all MOREFLAGS="-Werror"

.PHONY: clangbuild
clangbuild: clean
	clang -v
	CXX=clang++ CC=clang $(MAKE) all MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion -Wdocumentation"

m32build: clean
	gcc -v
	$(MAKE) all32

armbuild: clean
	CC=arm-linux-gnueabi-gcc CFLAGS="-Werror" $(MAKE) allzstd

aarch64build: clean
	CC=aarch64-linux-gnu-gcc CFLAGS="-Werror" $(MAKE) allzstd

ppcbuild: clean
	CC=powerpc-linux-gnu-gcc CFLAGS="-m32 -Wno-attributes -Werror" $(MAKE) allzstd

ppc64build: clean
	CC=powerpc-linux-gnu-gcc CFLAGS="-m64 -Werror" $(MAKE) allzstd

armfuzz: clean
	CC=arm-linux-gnueabi-gcc QEMU_SYS=qemu-arm-static MOREFLAGS="-static" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) fuzztest

aarch64fuzz: clean
	CC=aarch64-linux-gnu-gcc QEMU_SYS=qemu-aarch64-static MOREFLAGS="-static" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) fuzztest

ppcfuzz: clean
	CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc-static MOREFLAGS="-static" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) fuzztest

ppc64fuzz: clean
	CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc64-static MOREFLAGS="-m64 -static" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) fuzztest

.PHONY: cxxtest
cxxtest: CXXFLAGS += -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror
cxxtest: clean
	$(MAKE) -C $(PRGDIR) all CC="$(CXX) -Wno-deprecated" CFLAGS="$(CXXFLAGS)"   # adding -Wno-deprecated to avoid clang++ warning on dealing with C files directly

gcc5test: clean
	gcc-5 -v
	$(MAKE) all CC=gcc-5 MOREFLAGS="-Werror"

gcc6test: clean
	gcc-6 -v
	$(MAKE) all CC=gcc-6 MOREFLAGS="-Werror"

clangtest: clean
	clang -v
	$(MAKE) all CXX=clang-++ CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion -Wdocumentation"

armtest: clean
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=arm-linux-gnueabi-gcc QEMU_SYS=qemu-arm-static ZSTDRTTEST= MOREFLAGS="-Werror -static" FUZZER_FLAGS=--no-big-tests

aarch64test:
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=aarch64-linux-gnu-gcc QEMU_SYS=qemu-aarch64-static ZSTDRTTEST= MOREFLAGS="-Werror -static" FUZZER_FLAGS=--no-big-tests

ppctest: clean
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc-static ZSTDRTTEST= MOREFLAGS="-Werror -Wno-attributes -static" FUZZER_FLAGS=--no-big-tests

ppc64test: clean
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc64-static ZSTDRTTEST= MOREFLAGS="-m64 -static" FUZZER_FLAGS=--no-big-tests

arm-ppc-compilation:
	$(MAKE) -C $(PRGDIR) clean zstd CC=arm-linux-gnueabi-gcc QEMU_SYS=qemu-arm-static ZSTDRTTEST= MOREFLAGS="-Werror -static"
	$(MAKE) -C $(PRGDIR) clean zstd CC=aarch64-linux-gnu-gcc QEMU_SYS=qemu-aarch64-static ZSTDRTTEST= MOREFLAGS="-Werror -static"
	$(MAKE) -C $(PRGDIR) clean zstd CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc-static ZSTDRTTEST= MOREFLAGS="-Werror -Wno-attributes -static"
	$(MAKE) -C $(PRGDIR) clean zstd CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc64-static ZSTDRTTEST= MOREFLAGS="-m64 -static"

regressiontest:
	$(MAKE) -C $(FUZZDIR) regressiontest

uasanregressiontest:
	$(MAKE) -C $(FUZZDIR) regressiontest CC=clang CXX=clang++ CFLAGS="-O3 -fsanitize=address,undefined" CXXFLAGS="-O3 -fsanitize=address,undefined"

msanregressiontest:
	$(MAKE) -C $(FUZZDIR) regressiontest CC=clang CXX=clang++ CFLAGS="-O3 -fsanitize=memory" CXXFLAGS="-O3 -fsanitize=memory"

# run UBsan with -fsanitize-recover=signed-integer-overflow
# due to a bug in UBsan when doing pointer subtraction
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63303

usan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize-recover=signed-integer-overflow -fsanitize=undefined -Werror"

asan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=address -Werror"

asan-%: clean
	LDFLAGS=-fuse-ld=gold MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize=address -Werror" $(MAKE) -C $(TESTDIR) $*

msan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=memory -fno-omit-frame-pointer -Werror" HAVE_LZMA=0   # datagen.c fails this test for no obvious reason

msan-%: clean
	LDFLAGS=-fuse-ld=gold MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize=memory -fno-omit-frame-pointer -Werror" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) HAVE_LZMA=0 $*

asan32: clean
	$(MAKE) -C $(TESTDIR) test32 CC=clang MOREFLAGS="-g -fsanitize=address"

uasan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize-recover=signed-integer-overflow -fsanitize=address,undefined -Werror"

uasan-%: clean
	LDFLAGS=-fuse-ld=gold MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize-recover=signed-integer-overflow -fsanitize=address,undefined -Werror" $(MAKE) -C $(TESTDIR) $*

tsan-%: clean
	LDFLAGS=-fuse-ld=gold MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize=thread -Werror" $(MAKE) -C $(TESTDIR) $* FUZZER_FLAGS=--no-big-tests

apt-install:
	sudo apt-get -yq --no-install-suggests --no-install-recommends --force-yes install $(APT_PACKAGES)

apt-add-repo:
	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
	sudo apt-get update -y -qq

ppcinstall:
	APT_PACKAGES="qemu-system-ppc qemu-user-static gcc-powerpc-linux-gnu" $(MAKE) apt-install

arminstall:
	APT_PACKAGES="qemu-system-arm qemu-user-static gcc-arm-linux-gnueabi libc6-dev-armel-cross gcc-aarch64-linux-gnu libc6-dev-arm64-cross" $(MAKE) apt-install

valgrindinstall:
	APT_PACKAGES="valgrind" $(MAKE) apt-install

libc6install:
	APT_PACKAGES="libc6-dev-i386 gcc-multilib" $(MAKE) apt-install

gcc6install: apt-add-repo
	APT_PACKAGES="libc6-dev-i386 gcc-multilib gcc-6 gcc-6-multilib" $(MAKE) apt-install

gcc7install: apt-add-repo
	APT_PACKAGES="libc6-dev-i386 gcc-multilib gcc-7 gcc-7-multilib" $(MAKE) apt-install

gpp6install: apt-add-repo
	APT_PACKAGES="libc6-dev-i386 g++-multilib gcc-6 g++-6 g++-6-multilib" $(MAKE) apt-install

clang38install:
	APT_PACKAGES="clang-3.8" $(MAKE) apt-install

# Ubuntu 14.04 ships a too-old lz4
lz4install:
	[ -e lz4 ] || git clone https://github.com/lz4/lz4 && sudo $(MAKE) -C lz4 install

endif


ifneq (,$(filter MSYS%,$(shell uname)))
HOST_OS = MSYS
CMAKE_PARAMS = -G"MSYS Makefiles" -DZSTD_MULTITHREAD_SUPPORT:BOOL=OFF -DZSTD_BUILD_STATIC:BOOL=ON -DZSTD_BUILD_TESTS:BOOL=ON
endif


#------------------------------------------------------------------------
# target specific tests
#------------------------------------------------------------------------
ifneq (,$(filter $(HOST_OS),MSYS POSIX))
cmakebuild:
	cmake --version
	$(RM) -r $(BUILDIR)/cmake/build
	mkdir $(BUILDIR)/cmake/build
	cd $(BUILDIR)/cmake/build ; cmake -DCMAKE_INSTALL_PREFIX:PATH=~/install_test_dir $(CMAKE_PARAMS) .. ; $(MAKE) install ; $(MAKE) uninstall

c90build: clean
	$(CC) -v
	CFLAGS="-std=c90" $(MAKE) allmost  # will fail, due to missing support for `long long`

gnu90build: clean
	$(CC) -v
	CFLAGS="-std=gnu90" $(MAKE) allmost

c99build: clean
	$(CC) -v
	CFLAGS="-std=c99" $(MAKE) allmost

gnu99build: clean
	$(CC) -v
	CFLAGS="-std=gnu99" $(MAKE) allmost

c11build: clean
	$(CC) -v
	CFLAGS="-std=c11" $(MAKE) allmost

bmix64build: clean
	$(CC) -v
	CFLAGS="-O3 -mbmi -Werror" $(MAKE) -C $(TESTDIR) test

bmix32build: clean
	$(CC) -v
	CFLAGS="-O3 -mbmi -mx32 -Werror" $(MAKE) -C $(TESTDIR) test

bmi32build: clean
	$(CC) -v
	CFLAGS="-O3 -mbmi -m32 -Werror" $(MAKE) -C $(TESTDIR) test

staticAnalyze: clean
	$(CC) -v
	CPPFLAGS=-g scan-build --status-bugs -v $(MAKE) all
endif
