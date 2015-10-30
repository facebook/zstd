# ################################################################
# zstd - Makefile
# Copyright (C) Yann Collet 2014-2015
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
#  - zstd source repository : https://github.com/Cyan4973/zstd
#  - Public forum : https://groups.google.com/forum/#!forum/lz4c
# ################################################################

# Version number
export VERSION := 0.3.0

PRGDIR  = programs
ZSTDDIR = lib

.PHONY: clean

default: zstdprogram

all: 
	$(MAKE) -C $(ZSTDDIR) $@
	$(MAKE) -C $(PRGDIR) $@

zstdprogram:
	$(MAKE) -C $(PRGDIR)

clean:
	$(MAKE) -C $(ZSTDDIR) $@
	$(MAKE) -C $(PRGDIR) $@
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
	sudo $(MAKE) install

test:
	$(MAKE) -C $(PRGDIR) $@

clangtest: clean
	clang -v
	$(MAKE) all CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion"

gpptest: clean
	$(MAKE) all CC=g++ CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror"

armtest: clean
	$(MAKE) -C $(ZSTDDIR) -e all CC=arm-linux-gnueabi-gcc MOREFLAGS="-Werror"
	$(MAKE) -C $(PRGDIR) -e CC=arm-linux-gnueabi-gcc MOREFLAGS="-Werror"

usan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=undefined"

asan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=address"

uasan: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=address -fsanitize=undefined"

endif
