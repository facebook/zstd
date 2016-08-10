# ################################################################
# ZSTD library - Makefile
# Copyright (C) Yann Collet 2015-2016
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
#  - ZSTD homepage : http://www.zstd.net
# ################################################################

# Version numbers
LIBVER_MAJOR_SCRIPT:=`sed -n '/define ZSTD_VERSION_MAJOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < ./zstd.h`
LIBVER_MINOR_SCRIPT:=`sed -n '/define ZSTD_VERSION_MINOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < ./zstd.h`
LIBVER_PATCH_SCRIPT:=`sed -n '/define ZSTD_VERSION_RELEASE/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < ./zstd.h`
LIBVER_SCRIPT:= $(LIBVER_MAJOR_SCRIPT).$(LIBVER_MINOR_SCRIPT).$(LIBVER_PATCH_SCRIPT)
LIBVER_MAJOR := $(shell echo $(LIBVER_MAJOR_SCRIPT))
LIBVER_MINOR := $(shell echo $(LIBVER_MINOR_SCRIPT))
LIBVER_PATCH := $(shell echo $(LIBVER_PATCH_SCRIPT))
LIBVER := $(shell echo $(LIBVER_SCRIPT))
VERSION?= $(LIBVER)

DESTDIR?=
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

CPPFLAGS= -I. -I./common
CFLAGS ?= -O3
CFLAGS += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 \
          -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes -Wundef
FLAGS   = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(MOREFLAGS)


ZSTD_FILES := common/*.c compress/*.c decompress/*.c dictBuilder/*.c

ifeq ($(ZSTD_LEGACY_SUPPORT), 0)
CPPFLAGS  += -DZSTD_LEGACY_SUPPORT=0
else
ZSTD_FILES+= legacy/*.c
CPPFLAGS  += -I./legacy -DZSTD_LEGACY_SUPPORT=1
endif


# OS X linker doesn't support -soname, and use different extension
# see : https://developer.apple.com/library/mac/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/DynamicLibraryDesignGuidelines.html
ifeq ($(shell uname), Darwin)
	SHARED_EXT = dylib
	SHARED_EXT_MAJOR = $(LIBVER_MAJOR).$(SHARED_EXT)
	SHARED_EXT_VER = $(LIBVER).$(SHARED_EXT)
	SONAME_FLAGS = -install_name $(PREFIX)/lib/$@.$(SHARED_EXT_MAJOR) -compatibility_version $(LIBVER_MAJOR) -current_version $(LIBVER)
else
	SONAME_FLAGS = -Wl,-soname=$@.$(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT = so
	SHARED_EXT_MAJOR = $(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT_VER = $(SHARED_EXT).$(LIBVER)
endif


.PHONY: default all clean install uninstall

default: clean libzstd

all: clean libzstd

libzstd: $(ZSTD_FILES)
	@echo compiling static library
	@$(CC) $(FLAGS) -c $^
	@$(AR) rcs $@.a *.o
	@echo compiling dynamic library $(LIBVER)
	@$(CC) $(FLAGS) -shared $^ -fPIC $(SONAME_FLAGS) -o $@.$(SHARED_EXT_VER)
	@echo creating versioned links
	@ln -sf $@.$(SHARED_EXT_VER) $@.$(SHARED_EXT_MAJOR)
	@ln -sf $@.$(SHARED_EXT_VER) $@.$(SHARED_EXT)

clean:
	@rm -f core *.o *.a *.gcda *.$(SHARED_EXT) *.$(SHARED_EXT).* libzstd.pc
	@rm -f decompress/*.o
	@echo Cleaning library completed

#------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD, Hurd and some BSD targets
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU FreeBSD DragonFly))

libzstd.pc:
libzstd.pc: libzstd.pc.in
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
	@install -m 644 common/zbuff.h $(DESTDIR)$(INCLUDEDIR)/zbuff.h
	@install -m 644 dictBuilder/zdict.h $(DESTDIR)$(INCLUDEDIR)/zdict.h
	@echo zstd static and shared library installed

uninstall:
	@rm -f $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT)
	@rm -f $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR)
	@rm -f $(DESTDIR)$(LIBDIR)/pkgconfig/libzstd.pc
	@[ -x $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_VER) ] && rm -f $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_VER)
	@[ -f $(DESTDIR)$(LIBDIR)/libzstd.a ] && rm -f $(DESTDIR)$(LIBDIR)/libzstd.a
	@[ -f $(DESTDIR)$(INCLUDEDIR)/zstd.h ] && rm -f $(DESTDIR)$(INCLUDEDIR)/zstd.h
	@[ -f $(DESTDIR)$(INCLUDEDIR)/zbuff.h ] && rm -f $(DESTDIR)$(INCLUDEDIR)/zbuff.h
	@[ -f $(DESTDIR)$(INCLUDEDIR)/zdict.h ] && rm -f $(DESTDIR)$(INCLUDEDIR)/zdict.h
	@echo zstd libraries successfully uninstalled

endif
