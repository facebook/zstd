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
export VERSION=0.1.0
export RELEASE=r$(VERSION)

DESTDIR?=
PREFIX ?= /usr

LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include
PRGDIR  = programs
ZSTDDIR = lib

# Select test target for Travis CI's Build Matrix
ifneq (,$(filter test-%,$(ZSTD_TRAVIS_CI_ENV)))
TRAVIS_TARGET=prg-travis
else
TRAVIS_TARGET=$(ZSTD_TRAVIS_CI_ENV)
endif


.PHONY: clean

default: zstdprograms

all: 
	@cd $(ZSTDDIR); $(MAKE) -e all
	@cd $(PRGDIR); $(MAKE) -e all

zstdprograms:
	@cd $(PRGDIR); $(MAKE) -e

clean:
	@cd $(PRGDIR); $(MAKE) clean
	@cd $(ZSTDDIR); $(MAKE) clean
#	@cd examples; $(MAKE) clean
	@echo Cleaning completed


#------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD and Hurd targets
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU))

install:
	@cd $(ZSTDDIR); $(MAKE) -e install
	@cd $(PRGDIR); $(MAKE) -e install

uninstall:
	@cd $(ZSTDDIR); $(MAKE) uninstall
	@cd $(PRGDIR); $(MAKE) uninstall

travis-install:
	sudo $(MAKE) install

test:
	@cd $(PRGDIR); $(MAKE) -e test

test-travis: $(TRAVIS_TARGET)

prg-travis:
	@cd $(PRGDIR); $(MAKE) -e $(ZSTD_TRAVIS_CI_ENV)

clangtest: clean
	clang -v
	$(MAKE) all CC=clang MOREFLAGS="-Werror -Wconversion -Wno-sign-conversion"

gpptest: clean
	$(MAKE) all CC=g++ CFLAGS="-O3 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Werror"

armtest: clean
	cd $(ZSTDDIR); $(MAKE) -e all CC=arm-linux-gnueabi-gcc MOREFLAGS="-Werror"
	cd $(PRGDIR); $(MAKE) -e CC=arm-linux-gnueabi-gcc MOREFLAGS="-Werror"

sanitize: clean
	$(MAKE) test CC=clang MOREFLAGS="-g -fsanitize=undefined"

endif
