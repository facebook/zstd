# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ################################################################

PRGDIR  = programs
ZSTDDIR = lib
ZWRAPDIR = zlibWrapper
TESTDIR  = tests

# Define nul output
ifneq (,$(filter Windows%,$(OS)))
VOID = nul
else
VOID = /dev/null
endif

.PHONY: default all zlibwrapper zstd clean install uninstall travis-install test clangtest gpptest armtest usan asan uasan

default: zstd

all:
	$(MAKE) -C $(ZSTDDIR) $@
	$(MAKE) -C $(PRGDIR) $@ zstd32
	$(MAKE) -C $(TESTDIR) $@ all32

zstd:
	$(MAKE) -C $(PRGDIR)
	cp $(PRGDIR)/zstd .

zlibwrapper:
	$(MAKE) -C $(ZSTDDIR) all
	$(MAKE) -C $(ZWRAPDIR) all

test:
	$(MAKE) -C $(TESTDIR) $@

clean:
	@$(MAKE) -C $(ZSTDDIR) $@ > $(VOID)
	@$(MAKE) -C $(PRGDIR) $@ > $(VOID)
	@$(MAKE) -C $(TESTDIR) $@ > $(VOID)
	@$(MAKE) -C $(ZWRAPDIR) $@ > $(VOID)
	@rm -f zstd
	@echo Cleaning completed


#----------------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD, Hurd and some BSD targets
#----------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU FreeBSD DragonFly NetBSD))
HOST_OS = POSIX
install:
	$(MAKE) -C $(ZSTDDIR) $@
	$(MAKE) -C $(PRGDIR) $@

uninstall:
	$(MAKE) -C $(ZSTDDIR) $@
	$(MAKE) -C $(PRGDIR) $@

travis-install:
	$(MAKE) install PREFIX=~/install_test_dir

gpptest: clean
	$(MAKE) -C programs all CC=g++ CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror"

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
	$(MAKE) -C $(TESTDIR) test CC=arm-linux-gnueabi-gcc ZSTDRTTEST= MOREFLAGS="-Werror -static"

ppctest: clean
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=powerpc-linux-gnu-gcc ZSTDRTTEST= MOREFLAGS="-Werror -Wno-attributes -static"

ppc64test: clean
	$(MAKE) -C $(TESTDIR) datagen   # use native, faster
	$(MAKE) -C $(TESTDIR) test CC=powerpc-linux-gnu-gcc ZSTDRTTEST= MOREFLAGS="-m64 -static"

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
	rm -rf projects/cmake/build
	mkdir projects/cmake/build
	cd projects/cmake/build ; cmake -DPREFIX:STRING=~/install_test_dir $(CMAKE_PARAMS) .. ; $(MAKE) install ; $(MAKE) uninstall

c90test: clean
	CFLAGS="-std=c90" $(MAKE) all  # will fail, due to // and long long

gnu90test: clean
	CFLAGS="-std=gnu90" $(MAKE) all

c99test: clean
	CFLAGS="-std=c99" $(MAKE) all

gnu99test: clean
	CFLAGS="-std=gnu99" $(MAKE) all

c11test: clean
	CFLAGS="-std=c11" $(MAKE) all

bmix64test: clean
	CFLAGS="-O3 -mbmi -Werror" $(MAKE) -C $(TESTDIR) test

bmix32test: clean
	CFLAGS="-O3 -mbmi -mx32 -Werror" $(MAKE) -C $(TESTDIR) test

bmi32test: clean
	CFLAGS="-O3 -mbmi -m32 -Werror" $(MAKE) -C $(TESTDIR) test

staticAnalyze: clean
	CPPFLAGS=-g scan-build --status-bugs -v $(MAKE) all
endif
