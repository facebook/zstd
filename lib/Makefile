# ################################################################
# ZSTD library - Makefile
# Copyright (C) Yann Collet 2015
# All rights reserved.
# 
# BSD license

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
#  - ZSTD source repository : https://github.com/Cyan4973/zstd
#  - Public forum : https://groups.google.com/forum/#!forum/lz4c
# ################################################################

# Version numbers
VERSION?= 0
LIBVER_MAJOR=`sed -n '/define ZSTD_VERSION_MAJOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < zstd.h`
LIBVER_MINOR=`sed -n '/define ZSTD_VERSION_MINOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < zstd.h`
LIBVER_PATCH=`sed -n '/define ZSTD_VERSION_RELEASE/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < zstd.h`
LIBVER  = $(LIBVER_MAJOR).$(LIBVER_MINOR).$(LIBVER_PATCH)

DESTDIR?=
PREFIX ?= /usr
CFLAGS ?= -O3
CFLAGS += -I. -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes

LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include


# OS X linker doesn't support -soname, and use different extension
# see : https://developer.apple.com/library/mac/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/DynamicLibraryDesignGuidelines.html
ifeq ($(shell uname), Darwin)
	SHARED_EXT = dylib
	SHARED_EXT_MAJOR = $(LIBVER_MAJOR).$(SHARED_EXT)
	SHARED_EXT_VER = $(LIBVER).$(SHARED_EXT)
	SONAME_FLAGS = -install_name $(PREFIX)/lib/libzstd.$(SHARED_EXT_MAJOR) -compatibility_version $(LIBVER_MAJOR) -current_version $(LIBVER)
else
	SONAME_FLAGS = -Wl,-soname=libzstd.$(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT = so
	SHARED_EXT_MAJOR = $(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT_VER = $(SHARED_EXT).$(LIBVER)
endif

default: libzstd

all: libzstd

libzstd: zstd.c
	@echo compiling static library
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $^
	@$(AR) rcs libzstd.a zstd.o
	@echo compiling dynamic library $(LIBVER)
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -shared $^ -fPIC $(SONAME_FLAGS) -o $@.$(SHARED_EXT_VER)
	@echo creating versioned links
	@ln -sf $@.$(SHARED_EXT_VER) $@.$(SHARED_EXT_MAJOR)
	@ln -sf $@.$(SHARED_EXT_VER) $@.$(SHARED_EXT)

clean:
	@rm -f core *.o *.a *.$(SHARED_EXT) *.$(SHARED_EXT).* libzstd.pc
	@echo Cleaning library completed


#------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD and Hurd targets
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU))

libzstd.pc: libzstd.pc.in Makefile
	@echo creating pkgconfig
	@sed -e 's|@PREFIX@|$(PREFIX)|' \
            -e 's|@LIBDIR@|$(LIBDIR)|' \
            -e 's|@INCLUDEDIR@|$(INCLUDEDIR)|' \
            -e 's|@VERSION@|$(VERSION)|' \
             $< >$@

install: libzstd libzstd.pc
	@install -d -m 755 $(DESTDIR)$(LIBDIR)/pkgconfig/ $(DESTDIR)$(INCLUDEDIR)/
	@install -m 755 libzstd.$(SHARED_EXT_VER) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_VER)
	@cp -a libzstd.$(SHARED_EXT_MAJOR) $(DESTDIR)$(LIBDIR)
	@cp -a libzstd.$(SHARED_EXT) $(DESTDIR)$(LIBDIR)
	@cp -a libzstd.pc $(DESTDIR)$(LIBDIR)/pkgconfig/
	@install -m 644 libzstd.a $(DESTDIR)$(LIBDIR)/libzstd.a
	@install -m 644 zstd.h $(DESTDIR)$(INCLUDEDIR)/zstd.h
	@echo zstd static and shared library installed

uninstall:
	@rm -f $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT)
	@rm -f $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR)
	@rm -f $(DESTDIR)$(LIBDIR)/pkgconfig/libzstd.pc
	@[ -x $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_VER) ] && rm -f $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_VER)
	@[ -f $(DESTDIR)$(LIBDIR)/libzstd.a ] && rm -f $(DESTDIR)$(LIBDIR)/libzstd.a
	@[ -f $(DESTDIR)$(INCLUDEDIR)/zstd.h ] && rm -f $(DESTDIR)$(INCLUDEDIR)/zstd.h
	@echo zstd libraries successfully uninstalled

endif
