# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ################################################################

PRGDIR   = programs
ZSTDDIR  = lib
BUILDIR  = build
ZWRAPDIR = zlibWrapper
TESTDIR  = tests

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
all: | allmost examples manual

.PHONY: allmost
allmost:
	$(MAKE) -C $(ZSTDDIR) all
	$(MAKE) -C $(PRGDIR) all
	$(MAKE) -C $(TESTDIR) all
	$(MAKE) -C $(ZWRAPDIR) all

#skip zwrapper, can't build that on alternate architectures without the proper zlib installed
.PHONY: allarch
allarch:
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

.PHONY: shortest
shortest:
	$(MAKE) -C $(TESTDIR) $@

.PHONY: test
test:
	$(MAKE) -C $(TESTDIR) $@

.PHONY: examples
examples:
	CPPFLAGS=-I../lib LDFLAGS=-L../lib $(MAKE) -C examples/ all

.PHONY: manual
manual:
	$(MAKE) -C contrib/gen_html $@

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
	@$(RM) zstd$(EXT) zstdmt$(EXT) tmp*
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

.PHONY: install clangtest gpptest armtest usan asan uasan
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

.PHONY: gppbuild
gppbuild: clean
	g++ -v
	CC=g++ $(MAKE) -C programs all CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror"

.PHONY: gcc5build
gcc5build: clean
	gcc-5 -v
	CC=gcc-5 $(MAKE) all MOREFLAGS="-Werror"

.PHONY: gcc6build
gcc6build: clean
	gcc-6 -v
	CC=gcc-6 $(MAKE) all MOREFLAGS="-Werror"

.PHONY: clangbuild
clangbuild: clean
	clang -v
	CXX=clang++ CC=clang $(MAKE) all MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion -Wdocumentation"

m32build: clean
	gcc -v
	$(MAKE) all32

armbuild: clean
	CC=arm-linux-gnueabi-gcc CFLAGS="-Werror" $(MAKE) allarch

aarch64build: clean
	CC=aarch64-linux-gnu-gcc CFLAGS="-Werror" $(MAKE) allarch

ppcbuild: clean
	CC=powerpc-linux-gnu-gcc CLAGS="-m32 -Wno-attributes -Werror" $(MAKE) allarch

ppc64build: clean
	CC=powerpc-linux-gnu-gcc CFLAGS="-m64 -Werror" $(MAKE) allarch

armfuzz: clean
	CC=arm-linux-gnueabi-gcc QEMU_SYS=qemu-arm-static MOREFLAGS="-static" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) fuzztest

aarch64fuzz: clean
	CC=aarch64-linux-gnu-gcc QEMU_SYS=qemu-aarch64-static MOREFLAGS="-static" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) fuzztest

ppcfuzz: clean
	CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc-static MOREFLAGS="-static" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) fuzztest

ppc64fuzz: clean
	CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc64-static MOREFLAGS="-m64 -static" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) fuzztest

gpptest: clean
	CC=g++ $(MAKE) -C $(PRGDIR) all CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror"

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

# run UBsan with -fsanitize-recover=signed-integer-overflow
# due to a bug in UBsan when doing pointer subtraction
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63303

usan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize-recover=signed-integer-overflow -fsanitize=undefined"

asan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=address"

asan-%: clean
	LDFLAGS=-fuse-ld=gold MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize=address" $(MAKE) -C $(TESTDIR) $*

msan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=memory -fno-omit-frame-pointer" HAVE_LZMA=0   # datagen.c fails this test for no obvious reason

msan-%: clean
	LDFLAGS=-fuse-ld=gold MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize=memory -fno-omit-frame-pointer" FUZZER_FLAGS=--no-big-tests $(MAKE) -C $(TESTDIR) HAVE_LZMA=0 $*

asan32: clean
	$(MAKE) -C $(TESTDIR) test32 CC=clang MOREFLAGS="-g -fsanitize=address"

uasan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize-recover=signed-integer-overflow -fsanitize=address,undefined"

uasan-%: clean
	LDFLAGS=-fuse-ld=gold MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize-recover=signed-integer-overflow -fsanitize=address,undefined" $(MAKE) -C $(TESTDIR) $*

tsan-%: clean
	LDFLAGS=-fuse-ld=gold MOREFLAGS="-g -fno-sanitize-recover=all -fsanitize=thread" $(MAKE) -C $(TESTDIR) $* FUZZER_FLAGS=--no-big-tests

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

gpp6install: apt-add-repo
	APT_PACKAGES="libc6-dev-i386 g++-multilib gcc-6 g++-6 g++-6-multilib" $(MAKE) apt-install

clang38install:
	APT_PACKAGES="clang-3.8" $(MAKE) apt-install

endif


ifneq (,$(filter MSYS%,$(shell uname)))
HOST_OS = MSYS
CMAKE_PARAMS = -G"MSYS Makefiles" -DZSTD_MULTITHREAD_SUPPORT:BOOL=OFF -DZSTD_BUILD_STATIC:BOOL=ON -DZSTD_BUILD_TESTS:BOOL=ON
endif


#------------------------------------------------------------------------
#make tests validated only for MSYS, Linux, OSX, kFreeBSD and Hurd targets
#------------------------------------------------------------------------
ifneq (,$(filter $(HOST_OS),MSYS POSIX))
cmakebuild:
	cmake --version
	$(RM) -r $(BUILDIR)/cmake/build
	mkdir $(BUILDIR)/cmake/build
	cd $(BUILDIR)/cmake/build ; cmake -DCMAKE_INSTALL_PREFIX:PATH=~/install_test_dir $(CMAKE_PARAMS) .. ; $(MAKE) install ; $(MAKE) uninstall

c90build: clean
	gcc -v
	CFLAGS="-std=c90" $(MAKE) allmost  # will fail, due to missing support for `long long`

gnu90build: clean
	gcc -v
	CFLAGS="-std=gnu90" $(MAKE) allmost

c99build: clean
	gcc -v
	CFLAGS="-std=c99" $(MAKE) allmost

gnu99build: clean
	gcc -v
	CFLAGS="-std=gnu99" $(MAKE) allmost

c11build: clean
	gcc -v
	CFLAGS="-std=c11" $(MAKE) allmost

bmix64build: clean
	gcc -v
	CFLAGS="-O3 -mbmi -Werror" $(MAKE) -C $(TESTDIR) test

bmix32build: clean
	gcc -v
	CFLAGS="-O3 -mbmi -mx32 -Werror" $(MAKE) -C $(TESTDIR) test

bmi32build: clean
	gcc -v
	CFLAGS="-O3 -mbmi -m32 -Werror" $(MAKE) -C $(TESTDIR) test

staticAnalyze: clean
	gcc -v
	CPPFLAGS=-g scan-build --status-bugs -v $(MAKE) all
endif
