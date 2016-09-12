# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
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

default: libzstd

all: libzstd

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
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU FreeBSD DragonFly NetBSD))

libzstd.pc:
libzstd.pc: libzstd.pc.in
	@echo creating pkgconfig
	@sed -e 's|@PREFIX@|$(PREFIX)|' \
             -e 's|@LIBDIR@|$(LIBDIR)|' \
             -e 's|@INCLUDEDIR@|$(INCLUDEDIR)|' \
             -e 's|@VERSION@|$(VERSION)|' \
             $< >$@

install: libzstd.pc
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
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT)
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR)
	$(RM) $(DESTDIR)$(LIBDIR)/pkgconfig/libzstd.pc
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_VER)
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.a
	$(RM) $(DESTDIR)$(INCLUDEDIR)/zstd.h
	$(RM) $(DESTDIR)$(INCLUDEDIR)/zbuff.h
	$(RM) $(DESTDIR)$(INCLUDEDIR)/zdict.h
	@echo zstd libraries successfully uninstalled

endif
