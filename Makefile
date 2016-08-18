# ################################################################
# zstd - Makefile
# Copyright (C) Yann Collet 2014-2016
# All rights reserved.
#
# BSD license
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice, this
#   list of conditions and the following disclaimer in the documentation and/or
#   other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# You can contact the author at :
#  - zstd homepage : http://www.zstd.net/
# ################################################################

PRGDIR  = programs
ZSTDDIR = lib
ZWRAPDIR = zlibWrapper

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
	$(MAKE) -C $(PRGDIR) $@
	@rm -f lib/decompress/*.o
	$(MAKE) -C $(PRGDIR) all32

zstd:
	$(MAKE) -C $(PRGDIR)
	cp $(PRGDIR)/zstd .

zlibwrapper:
	$(MAKE) -C $(ZSTDDIR) all
	$(MAKE) -C $(ZWRAPDIR) all

test:
	$(MAKE) -C $(PRGDIR) $@

clean:
	@$(MAKE) -C $(ZSTDDIR) $@ > $(VOID)
	@$(MAKE) -C $(PRGDIR) $@ > $(VOID)
	@$(MAKE) -C $(ZWRAPDIR) $@ > $(VOID)
	@rm -f zstd
	@echo Cleaning completed


#----------------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD, Hurd and some BSD targets
#----------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU FreeBSD DragonFly))
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
	$(MAKE) all CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion"

armtest: clean
	$(MAKE) -C $(PRGDIR) datagen   # use native, faster
	$(MAKE) -C $(PRGDIR) test CC=arm-linux-gnueabi-gcc ZSTDRTTEST= MOREFLAGS="-Werror -static"

ppctest: clean
	$(MAKE) -C $(PRGDIR) datagen   # use native, faster
	$(MAKE) -C $(PRGDIR) test CC=powerpc-linux-gnu-gcc ZSTDRTTEST= MOREFLAGS="-Werror -Wno-attributes -static"

ppc64test: clean
	$(MAKE) -C $(PRGDIR) datagen   # use native, faster
	$(MAKE) -C $(PRGDIR) test CC=powerpc-linux-gnu-gcc ZSTDRTTEST= MOREFLAGS="-m64 -static"

usan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=undefined"

asan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=address"

msan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=memory -fno-omit-frame-pointer"   # datagen.c fails this test for no obvious reason

asan32: clean
	$(MAKE) -C $(PRGDIR) test32 CC=clang MOREFLAGS="-g -fsanitize=address"

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
	CFLAGS="-O3 -mbmi -Werror" $(MAKE) -C $(PRGDIR) test

bmix32test: clean
	CFLAGS="-O3 -mbmi -mx32 -Werror" $(MAKE) -C $(PRGDIR) test

bmi32test: clean
	CFLAGS="-O3 -mbmi -m32 -Werror" $(MAKE) -C $(PRGDIR) test

staticAnalyze: clean
	CPPFLAGS=-g scan-build --status-bugs -v $(MAKE) all
endif
