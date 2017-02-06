# ##########################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This Makefile is validated for Linux, macOS, *BSD, Hurd, Solaris, MSYS2 targets
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ##########################################################################

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

CPPFLAGS+= -I. -I./common -DXXH_NAMESPACE=ZSTD_
CFLAGS  ?= -O3
DEBUGFLAGS = -g -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
           -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
           -Wstrict-prototypes -Wundef -Wpointer-arith
CFLAGS  += $(DEBUGFLAGS) $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CFLAGS)


ZSTD_FILES := $(wildcard common/*.c compress/*.c decompress/*.c dictBuilder/*.c deprecated/*.c)

ifeq ($(ZSTD_LEGACY_SUPPORT), 0)
CPPFLAGS  += -DZSTD_LEGACY_SUPPORT=0
else
CPPFLAGS  += -I./legacy -DZSTD_LEGACY_SUPPORT=1
ZSTD_FILES+= $(wildcard legacy/*.c)
endif

ZSTD_OBJ   := $(patsubst %.c,%.o,$(ZSTD_FILES))

# OS X linker doesn't support -soname, and use different extension
# see : https://developer.apple.com/library/mac/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/DynamicLibraryDesignGuidelines.html
ifeq ($(shell uname), Darwin)
	SHARED_EXT = dylib
	SHARED_EXT_MAJOR = $(LIBVER_MAJOR).$(SHARED_EXT)
	SHARED_EXT_VER = $(LIBVER).$(SHARED_EXT)
	SONAME_FLAGS = -install_name $(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR) -compatibility_version $(LIBVER_MAJOR) -current_version $(LIBVER)
else
	SONAME_FLAGS = -Wl,-soname=libzstd.$(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT = so
	SHARED_EXT_MAJOR = $(SHARED_EXT).$(LIBVER_MAJOR)
	SHARED_EXT_VER = $(SHARED_EXT).$(LIBVER)
endif

LIBZSTD = libzstd.$(SHARED_EXT_VER)


.PHONY: default all clean install uninstall

default: lib-release

all: lib

libzstd.a: ARFLAGS = rcs
libzstd.a: $(ZSTD_OBJ)
	@echo compiling static library
	@$(AR) $(ARFLAGS) $@ $^

$(LIBZSTD): LDFLAGS += -shared -fPIC -fvisibility=hidden
$(LIBZSTD): $(ZSTD_FILES)
	@echo compiling dynamic library $(LIBVER)
ifneq (,$(filter Windows%,$(OS)))
	@$(CC) $(FLAGS) -DZSTD_DLL_EXPORT=1 -shared $^ -o dll\libzstd.dll
	dlltool -D dll\libzstd.dll -d dll\libzstd.def -l dll\libzstd.lib
else
	@$(CC) $(FLAGS) $^ $(LDFLAGS) $(SONAME_FLAGS) -o $@
	@echo creating versioned links
	@ln -sf $@ libzstd.$(SHARED_EXT_MAJOR)
	@ln -sf $@ libzstd.$(SHARED_EXT)
endif

libzstd : $(LIBZSTD)

lib: libzstd.a libzstd

lib-release: DEBUGFLAGS :=
lib-release: lib

clean:
	@$(RM) -r *.dSYM   # Mac OS-X specific
	@$(RM) core *.o *.a *.gcda *.$(SHARED_EXT) *.$(SHARED_EXT).* libzstd.pc
	@$(RM) dll/libzstd.dll dll/libzstd.lib
	@$(RM) common/*.o compress/*.o decompress/*.o dictBuilder/*.o legacy/*.o deprecated/*.o
	@echo Cleaning library completed

#-----------------------------------------------------------------------------
# make install is validated only for Linux, OSX, BSD, Hurd and Solaris targets
#-----------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS))

ifneq (,$(filter $(shell uname),SunOS))
INSTALL ?= ginstall
else
INSTALL ?= install
endif

PREFIX     ?= /usr/local
DESTDIR    ?=
LIBDIR     ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include

ifneq (,$(filter $(shell uname),OpenBSD FreeBSD NetBSD DragonFly))
PKGCONFIGDIR ?= $(PREFIX)/libdata/pkgconfig
else
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
endif

INSTALL_LIB  ?= $(INSTALL) -m 755
INSTALL_DATA ?= $(INSTALL) -m 644


libzstd.pc:
libzstd.pc: libzstd.pc.in
	@echo creating pkgconfig
	@sed -e 's|@PREFIX@|$(PREFIX)|' \
             -e 's|@LIBDIR@|$(LIBDIR)|' \
             -e 's|@INCLUDEDIR@|$(INCLUDEDIR)|' \
             -e 's|@VERSION@|$(VERSION)|' \
             $< >$@

install: libzstd.a libzstd libzstd.pc
	@$(INSTALL) -d -m 755 $(DESTDIR)$(PKGCONFIGDIR)/ $(DESTDIR)$(INCLUDEDIR)/
	@$(INSTALL_DATA) libzstd.pc $(DESTDIR)$(PKGCONFIGDIR)/
	@echo Installing libraries
	@$(INSTALL_LIB) libzstd.a $(DESTDIR)$(LIBDIR)
	@$(INSTALL_LIB) libzstd.$(SHARED_EXT_VER) $(DESTDIR)$(LIBDIR)
	@ln -sf libzstd.$(SHARED_EXT_VER) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR)
	@ln -sf libzstd.$(SHARED_EXT_VER) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT)
	@echo Installing includes
	@$(INSTALL_DATA) zstd.h $(DESTDIR)$(INCLUDEDIR)
	@$(INSTALL_DATA) common/zstd_errors.h $(DESTDIR)$(INCLUDEDIR)
	@$(INSTALL_DATA) deprecated/zbuff.h $(DESTDIR)$(INCLUDEDIR)     # prototypes generate deprecation warnings
	@$(INSTALL_DATA) dictBuilder/zdict.h $(DESTDIR)$(INCLUDEDIR)
	@echo zstd static and shared library installed

uninstall:
	@$(RM) $(DESTDIR)$(LIBDIR)/libzstd.a
	@$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT)
	@$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR)
	@$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_VER)
	@$(RM) $(DESTDIR)$(PKGCONFIGDIR)/libzstd.pc
	@$(RM) $(DESTDIR)$(INCLUDEDIR)/zstd.h
	@$(RM) $(DESTDIR)$(INCLUDEDIR)/zstd_errors.h
	@$(RM) $(DESTDIR)$(INCLUDEDIR)/zbuff.h   # Deprecated streaming functions
	@$(RM) $(DESTDIR)$(INCLUDEDIR)/zdict.h
	@echo zstd libraries successfully uninstalled

endif
