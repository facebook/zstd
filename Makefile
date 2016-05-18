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

# force a version number : uncomment below export (otherwise, default to the one declared into zstd.h)
#export VERSION := 0.6.1

PRGDIR  = programs
ZSTDDIR = lib

# Define nul output
ifneq (,$(filter Windows%,$(OS)))
VOID = nul
else
VOID = /dev/null
endif

.PHONY: default all zstdprogram clean install uninstall travis-install test clangtest gpptest armtest usan asan uasan

default: zstdprogram

all: 
	$(MAKE) -C $(ZSTDDIR) $@
	$(MAKE) -C $(PRGDIR) $@

zstdprogram:
	$(MAKE) -C $(PRGDIR)

test:
	$(MAKE) -C $(PRGDIR) $@

clean:
	@$(MAKE) -C $(ZSTDDIR) $@ > $(VOID)
	@$(MAKE) -C $(PRGDIR) $@ > $(VOID)
	@echo Cleaning completed


#------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD and Hurd targets
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU))

install:
	$(MAKE) -C $(ZSTDDIR) $@
	$(MAKE) -C $(PRGDIR) $@

uninstall:
	$(MAKE) -C $(ZSTDDIR) $@
	$(MAKE) -C $(PRGDIR) $@

travis-install:
	$(MAKE) install PREFIX=~/install_test_dir

cmaketest:
	cd contrib/cmake ; cmake . ; $(MAKE)

clangtest: clean
	clang -v
	$(MAKE) all CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion"

gpptest: clean
	$(MAKE) all CC=g++ CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror"

gnu90test: clean
	$(MAKE) all CFLAGS="-std=gnu90 -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 -Wswitch-enum -Wstrict-prototypes -Wundef -Wdeclaration-after-statement -Werror"

c90test: clean
	$(MAKE) all CFLAGS="-std=c90 -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 -Wswitch-enum -Wstrict-prototypes -Wundef -Werror"   # will fail, due to // and long long

bmix64test: clean
	CFLAGS="-O3 -mbmi -Werror" $(MAKE) -C $(PRGDIR) test

bmix32test: clean
	CFLAGS="-O3 -mbmi -mx32 -Werror" $(MAKE) -C $(PRGDIR) test

bmi32test: clean
	CFLAGS="-O3 -mbmi -m32 -Werror" $(MAKE) -C $(PRGDIR) test

armtest: clean
	$(MAKE) -C $(PRGDIR) datagen   # use native, faster
	$(MAKE) -C $(PRGDIR) test CC=arm-linux-gnueabi-gcc ZSTDRTTEST= MOREFLAGS="-Werror -static"

# for Travis CI
arminstall: clean   
	sudo apt-get install -q qemu  
	sudo apt-get install -q binfmt-support
	sudo apt-get install -q qemu-user-static
	sudo apt-get install -q gcc-arm-linux-gnueabi

# for Travis CI
armtest-w-install: clean arminstall armtest

ppctest: clean
	$(MAKE) -C $(PRGDIR) datagen   # use native, faster
	$(MAKE) -C $(PRGDIR) test CC=powerpc-linux-gnu-gcc ZSTDRTTEST= MOREFLAGS="-Werror -static" 

# for Travis CI
ppcinstall: clean   
	sudo apt-get install -q qemu  
	sudo apt-get install -q binfmt-support
	sudo apt-get install -q qemu-user-static
	sudo apt-get update  -q
	sudo apt-get install -q gcc-powerpc-linux-gnu   # unfortunately, doesn't work on Travis CI (package not available)

# for Travis CI
ppctest-w-install: clean ppcinstall ppctest

usan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=undefined"

asan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=address"

msan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=memory"   # datagen.c fails this test, for no obvious reason

asan32: clean
	$(MAKE) -C $(PRGDIR) test32 CC=clang MOREFLAGS="-g -fsanitize=address"

uasan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=address -fsanitize=undefined"

endif
