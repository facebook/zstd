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
default: lib zstd-release

.PHONY: all
all: allmost
	CPPFLAGS=-I../lib LDFLAGS=-L../lib $(MAKE) -C examples/ $@

.PHONY: allmost
allmost:
	$(MAKE) -C $(ZSTDDIR) all
	$(MAKE) -C $(PRGDIR) all
	$(MAKE) -C $(TESTDIR) all
	$(MAKE) -C $(ZWRAPDIR) all

.PHONY: all32
all32:
	$(MAKE) -C $(PRGDIR) zstd32
	$(MAKE) -C $(TESTDIR) all32

.PHONY: lib
lib:
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
	$(MAKE) -C $(TESTDIR) $@

.PHONY: clean
clean:
	@$(MAKE) -C $(ZSTDDIR) $@ > $(VOID)
	@$(MAKE) -C $(PRGDIR) $@ > $(VOID)
	@$(MAKE) -C $(TESTDIR) $@ > $(VOID)
	@$(MAKE) -C $(ZWRAPDIR) $@ > $(VOID)
	@$(MAKE) -C examples/ $@ > $(VOID)
	@$(RM) zstd$(EXT) zstdmt$(EXT) tmp*
	@echo Cleaning completed


#------------------------------------------------------------------------------
# make install is validated only for Linux, OSX, Hurd and some BSD targets
#------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU FreeBSD DragonFly NetBSD))
HOST_OS = POSIX
.PHONY: install uninstall travis-install clangtest gpptest armtest usan asan uasan

install:
	@$(MAKE) -C $(ZSTDDIR) $@
	@$(MAKE) -C $(PRGDIR) $@

uninstall:
	@$(MAKE) -C $(ZSTDDIR) $@
	@$(MAKE) -C $(PRGDIR) $@

travis-install:
	$(MAKE) install PREFIX=~/install_test_dir

gpptest: clean
	CC=g++ $(MAKE) -C programs all CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror"

gcc5test: clean
	gcc-5 -v
	$(MAKE) all CC=gcc-5 MOREFLAGS="-Werror"

gcc6test: clean
	gcc-6 -v
	$(MAKE) all CC=gcc-6 MOREFLAGS="-Werror"

clangtest: clean
	clang -v
	$(MAKE) all CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion -Wdocumentation"

armtest: clean
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=arm-linux-gnueabi-gcc QEMU_SYS=qemu-arm-static ZSTDRTTEST= MOREFLAGS="-Werror -static"

aarch64test:
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=aarch64-linux-gnu-gcc QEMU_SYS=qemu-aarch64-static ZSTDRTTEST= MOREFLAGS="-Werror -static"

ppctest: clean
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc-static ZSTDRTTEST= MOREFLAGS="-Werror -Wno-attributes -static"

ppc64test: clean
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=powerpc-linux-gnu-gcc QEMU_SYS=qemu-ppc64-static ZSTDRTTEST= MOREFLAGS="-m64 -static"

usan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=undefined"

asan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=address"

msan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=memory -fno-omit-frame-pointer"   # datagen.c fails this test for no obvious reason

asan32: clean
	$(MAKE) -C $(TESTDIR) test32 CC=clang MOREFLAGS="-g -fsanitize=address"

uasan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=address -fsanitize=undefined"

endif


ifneq (,$(filter MSYS%,$(shell uname)))
HOST_OS = MSYS
CMAKE_PARAMS = -G"MSYS Makefiles"
endif


#------------------------------------------------------------------------
#make tests validated only for MSYS, Linux, OSX, kFreeBSD and Hurd targets
#------------------------------------------------------------------------
ifneq (,$(filter $(HOST_OS),MSYS POSIX))
cmaketest:
	cmake --version
	$(RM) -r $(BUILDIR)/cmake/build
	mkdir $(BUILDIR)/cmake/build
	cd $(BUILDIR)/cmake/build ; cmake -DPREFIX:STRING=~/install_test_dir $(CMAKE_PARAMS) .. ; $(MAKE) install ; $(MAKE) uninstall

c90test: clean
	CFLAGS="-std=c90" $(MAKE) all  # will fail, due to // and long long

gnu90test: clean
	CFLAGS="-std=gnu90" $(MAKE) all

c99test: clean
	CFLAGS="-std=c99" $(MAKE) allmost

gnu99test: clean
	CFLAGS="-std=gnu99" $(MAKE) all

c11test: clean
	CFLAGS="-std=c11" $(MAKE) allmost

bmix64test: clean
	CFLAGS="-O3 -mbmi -Werror" $(MAKE) -C $(TESTDIR) test

bmix32test: clean
	CFLAGS="-O3 -mbmi -mx32 -Werror" $(MAKE) -C $(TESTDIR) test

bmi32test: clean
	CFLAGS="-O3 -mbmi -m32 -Werror" $(MAKE) -C $(TESTDIR) test

staticAnalyze: clean
	CPPFLAGS=-g scan-build --status-bugs -v $(MAKE) all
endif
