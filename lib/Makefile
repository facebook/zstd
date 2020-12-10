# ################################################################
# Copyright (c) 2015-2020, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
# ################################################################

.PHONY: default
default: lib-release

# define silent mode as default (verbose mode with V=1 or VERBOSE=1)
$(V)$(VERBOSE).SILENT:

# When cross-compiling from linux to windows,
# one might need to specify TARGET_SYSTEM as "Windows."
# Building from Fedora fails without it.
# (but Ubuntu and Debian don't need to set anything)
TARGET_SYSTEM ?= $(OS)

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
CCVER := $(shell $(CC) --version)

# ZSTD_LIB_MINIFY is a helper variable that
# configures a bunch of other variables to space-optimized defaults.
ZSTD_LIB_MINIFY ?= 0
ifneq ($(ZSTD_LIB_MINIFY), 0)
  HAVE_CC_OZ ?= $(shell echo "" | $(CC) -Oz -x c -c - -o /dev/null 2> /dev/null && echo 1 || echo 0)
  ZSTD_LEGACY_SUPPORT ?= 0
  ZSTD_LIB_DEPRECATED ?= 0
  HUF_FORCE_DECOMPRESS_X1 ?= 1
  ZSTD_FORCE_DECOMPRESS_SHORT ?= 1
  ZSTD_NO_INLINE ?= 1
  ZSTD_STRIP_ERROR_STRINGS ?= 1
ifneq ($(HAVE_CC_OZ), 0)
    # Some compilers (clang) support an even more space-optimized setting.
    CFLAGS += -Oz
else
    CFLAGS += -Os
endif
  CFLAGS += -fno-stack-protector -fomit-frame-pointer -fno-ident \
            -DDYNAMIC_BMI2=0 -DNDEBUG
else
  CFLAGS += -O3
endif

DEBUGLEVEL ?= 0
CPPFLAGS += -DXXH_NAMESPACE=ZSTD_ -DDEBUGLEVEL=$(DEBUGLEVEL)
ifeq ($(TARGET_SYSTEM),Windows_NT)   # MinGW assumed
  CPPFLAGS += -D__USE_MINGW_ANSI_STDIO   # compatibility with %zu formatting
endif
DEBUGFLAGS= -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
            -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wundef -Wpointer-arith \
            -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            -Wredundant-decls -Wmissing-prototypes -Wc++-compat
CFLAGS   += $(DEBUGFLAGS) $(MOREFLAGS)
FLAGS     = $(CPPFLAGS) $(CFLAGS)

HAVE_COLORNEVER = $(shell echo a | grep --color=never a > /dev/null 2> /dev/null && echo 1 || echo 0)
GREP_OPTIONS ?=
ifeq ($HAVE_COLORNEVER, 1)
  GREP_OPTIONS += --color=never
endif
GREP = grep $(GREP_OPTIONS)
SED_ERE_OPT ?= -E

ZSTDCOMMON_FILES := $(sort $(wildcard common/*.c))
ZSTDCOMP_FILES := $(sort $(wildcard compress/*.c))
ZSTDDECOMP_FILES := $(sort $(wildcard decompress/*.c))
ZDICT_FILES := $(sort $(wildcard dictBuilder/*.c))
ZDEPR_FILES := $(sort $(wildcard deprecated/*.c))
ZSTD_FILES := $(ZSTDCOMMON_FILES)

ifeq ($(findstring GCC,$(CCVER)),GCC)
decompress/zstd_decompress_block.o : CFLAGS+=-fno-tree-vectorize
endif

# Modules
ZSTD_LIB_COMPRESSION ?= 1
ZSTD_LIB_DECOMPRESSION ?= 1
ZSTD_LIB_DICTBUILDER ?= 1
ZSTD_LIB_DEPRECATED ?= 1

# Legacy support
ZSTD_LEGACY_SUPPORT ?= 5
ZSTD_LEGACY_MULTITHREADED_API ?= 0

# Build size optimizations
HUF_FORCE_DECOMPRESS_X1 ?= 0
HUF_FORCE_DECOMPRESS_X2 ?= 0
ZSTD_FORCE_DECOMPRESS_SHORT ?= 0
ZSTD_FORCE_DECOMPRESS_LONG ?= 0
ZSTD_NO_INLINE ?= 0
ZSTD_STRIP_ERROR_STRINGS ?= 0

ifeq ($(ZSTD_LIB_COMPRESSION), 0)
  ZSTD_LIB_DICTBUILDER = 0
  ZSTD_LIB_DEPRECATED = 0
endif

ifeq ($(ZSTD_LIB_DECOMPRESSION), 0)
  ZSTD_LEGACY_SUPPORT = 0
  ZSTD_LIB_DEPRECATED = 0
endif

ifneq ($(ZSTD_LIB_COMPRESSION), 0)
  ZSTD_FILES += $(ZSTDCOMP_FILES)
endif

ifneq ($(ZSTD_LIB_DECOMPRESSION), 0)
  ZSTD_FILES += $(ZSTDDECOMP_FILES)
endif

ifneq ($(ZSTD_LIB_DEPRECATED), 0)
  ZSTD_FILES += $(ZDEPR_FILES)
endif

ifneq ($(ZSTD_LIB_DICTBUILDER), 0)
  ZSTD_FILES += $(ZDICT_FILES)
endif

ifneq ($(HUF_FORCE_DECOMPRESS_X1), 0)
  CFLAGS += -DHUF_FORCE_DECOMPRESS_X1
endif

ifneq ($(HUF_FORCE_DECOMPRESS_X2), 0)
  CFLAGS += -DHUF_FORCE_DECOMPRESS_X2
endif

ifneq ($(ZSTD_FORCE_DECOMPRESS_SHORT), 0)
  CFLAGS += -DZSTD_FORCE_DECOMPRESS_SHORT
endif

ifneq ($(ZSTD_FORCE_DECOMPRESS_LONG), 0)
  CFLAGS += -DZSTD_FORCE_DECOMPRESS_LONG
endif

ifneq ($(ZSTD_NO_INLINE), 0)
  CFLAGS += -DZSTD_NO_INLINE
endif

ifneq ($(ZSTD_STRIP_ERROR_STRINGS), 0)
  CFLAGS += -DZSTD_STRIP_ERROR_STRINGS
endif

ifneq ($(ZSTD_LEGACY_MULTITHREADED_API), 0)
  CFLAGS += -DZSTD_LEGACY_MULTITHREADED_API
endif

ifneq ($(ZSTD_LEGACY_SUPPORT), 0)
ifeq ($(shell test $(ZSTD_LEGACY_SUPPORT) -lt 8; echo $$?), 0)
  ZSTD_FILES += $(shell ls legacy/*.c | $(GREP) 'v0[$(ZSTD_LEGACY_SUPPORT)-7]')
endif
endif
CPPFLAGS  += -DZSTD_LEGACY_SUPPORT=$(ZSTD_LEGACY_SUPPORT)

ZSTD_LOCAL_SRC := $(notdir $(ZSTD_FILES))
ZSTD_LOCAL_OBJ := $(ZSTD_LOCAL_SRC:.c=.o)

ZSTD_SUBDIR := common compress decompress dictBuilder legacy deprecated
vpath %.c $(ZSTD_SUBDIR)

UNAME := $(shell uname)

ifndef BUILD_DIR
ifeq ($(UNAME), Darwin)
  HASH ?= md5
else ifeq ($(UNAME), FreeBSD)
  HASH ?= gmd5sum
else ifeq ($(UNAME), OpenBSD)
  HASH ?= md5
endif
HASH ?= md5sum

HASH_DIR = conf_$(shell echo $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(ZSTD_FILES) | $(HASH) | cut -f 1 -d " " )
HAVE_HASH :=$(shell echo 1 | $(HASH) > /dev/null && echo 1 || echo 0)
ifeq ($(HAVE_HASH),0)
  $(info warning : could not find HASH ($(HASH)), needed to differentiate builds using different flags)
  BUILD_DIR := obj/generic_noconf
endif
endif # BUILD_DIR


# macOS linker doesn't support -soname, and use different extension
# see : https://developer.apple.com/library/mac/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/DynamicLibraryDesignGuidelines.html
ifeq ($(UNAME), Darwin)
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

SET_CACHE_DIRECTORY = \
	$(MAKE) --no-print-directory $@ \
    BUILD_DIR=obj/$(HASH_DIR) \
    CPPFLAGS="$(CPPFLAGS)" \
    CFLAGS="$(CFLAGS)" \
    LDFLAGS="$(LDFLAGS)"


.PHONY: lib-all all clean install uninstall

# alias
lib-all: all

all: lib

.PHONY: libzstd.a  # must be run every time

ifndef BUILD_DIR
# determine BUILD_DIR from compilation flags

libzstd.a:
	$(SET_CACHE_DIRECTORY)

else
# BUILD_DIR is defined

ZSTD_STATLIB_DIR := $(BUILD_DIR)/static
ZSTD_STATLIB := $(ZSTD_STATLIB_DIR)/libzstd.a
ZSTD_STATLIB_OBJ := $(addprefix $(ZSTD_STATLIB_DIR)/,$(ZSTD_LOCAL_OBJ))
$(ZSTD_STATLIB): ARFLAGS = rcs
$(ZSTD_STATLIB): | $(ZSTD_STATLIB_DIR)
$(ZSTD_STATLIB): $(ZSTD_STATLIB_OBJ)
	@echo compiling static library
	$(AR) $(ARFLAGS) $@ $^

libzstd.a: $(ZSTD_STATLIB)
	cp -f $< $@

endif

ifneq (,$(filter Windows%,$(TARGET_SYSTEM)))

LIBZSTD = dll/libzstd.dll
$(LIBZSTD): $(ZSTD_FILES)
	@echo compiling dynamic library $(LIBVER)
	$(CC) $(FLAGS) -DZSTD_DLL_EXPORT=1 -Wl,--out-implib,dll/libzstd.dll.a -shared $^ -o $@

else  # not Windows

LIBZSTD = libzstd.$(SHARED_EXT_VER)
.PHONY: $(LIBZSTD)  # must be run every time
$(LIBZSTD): CFLAGS += -fPIC
$(LIBZSTD): LDFLAGS += -shared -fvisibility=hidden

ifndef BUILD_DIR
# determine BUILD_DIR from compilation flags

$(LIBZSTD):
	$(SET_CACHE_DIRECTORY)

else
# BUILD_DIR is defined

ZSTD_DYNLIB_DIR := $(BUILD_DIR)/dynamic
ZSTD_DYNLIB := $(ZSTD_DYNLIB_DIR)/$(LIBZSTD)
ZSTD_DYNLIB_OBJ := $(addprefix $(ZSTD_DYNLIB_DIR)/,$(ZSTD_LOCAL_OBJ))

$(ZSTD_DYNLIB): | $(ZSTD_DYNLIB_DIR)
$(ZSTD_DYNLIB): $(ZSTD_DYNLIB_OBJ)
	@echo compiling dynamic library $(LIBVER)
	$(CC) $(FLAGS) $^ $(LDFLAGS) $(SONAME_FLAGS) -o $@
	@echo creating versioned links
	ln -sf $@ libzstd.$(SHARED_EXT_MAJOR)
	ln -sf $@ libzstd.$(SHARED_EXT)

$(LIBZSTD): $(ZSTD_DYNLIB)
	cp -f $< $@

endif  # ifndef BUILD_DIR
endif  # if windows

.PHONY: libzstd
libzstd : $(LIBZSTD)

.PHONY: lib
lib : libzstd.a libzstd


# note : do not define lib-mt or lib-release as .PHONY
# make does not consider implicit pattern rule for .PHONY target

%-mt : CPPFLAGS += -DZSTD_MULTITHREAD
%-mt : LDFLAGS  += -pthread
%-mt : %
	@echo multi-threading build completed

%-release : DEBUGFLAGS :=
%-release : %
	@echo release build completed


# Generate .h dependencies automatically

DEPFLAGS = -MT $@ -MMD -MP -MF

$(ZSTD_DYNLIB_DIR)/%.o : %.c $(ZSTD_DYNLIB_DIR)/%.d | $(ZSTD_DYNLIB_DIR)
	@echo CC $@
	$(COMPILE.c) $(DEPFLAGS) $(ZSTD_DYNLIB_DIR)/$*.d $(OUTPUT_OPTION) $<

$(ZSTD_STATLIB_DIR)/%.o : %.c $(ZSTD_STATLIB_DIR)/%.d | $(ZSTD_STATLIB_DIR)
	@echo CC $@
	$(COMPILE.c) $(DEPFLAGS) $(ZSTD_STATLIB_DIR)/$*.d $(OUTPUT_OPTION) $<

MKDIR ?= mkdir
$(BUILD_DIR) $(ZSTD_DYNLIB_DIR) $(ZSTD_STATLIB_DIR):
	$(MKDIR) -p $@

DEPFILES := $(ZSTD_DYNLIB_OBJ:.o=.d) $(ZSTD_STATLIB_OBJ:.o=.d)
$(DEPFILES):

include $(wildcard $(DEPFILES))


# Special case : building library in single-thread mode _and_ without zstdmt_compress.c
ZSTDMT_FILES = compress/zstdmt_compress.c
ZSTD_NOMT_FILES = $(filter-out $(ZSTDMT_FILES),$(ZSTD_FILES))
libzstd-nomt: LDFLAGS += -shared -fPIC -fvisibility=hidden
libzstd-nomt: $(ZSTD_NOMT_FILES)
	@echo compiling single-thread dynamic library $(LIBVER)
	@echo files : $(ZSTD_NOMT_FILES)
	$(CC) $(FLAGS) $^ $(LDFLAGS) $(SONAME_FLAGS) -o $@

clean:
	$(RM) -r *.dSYM   # macOS-specific
	$(RM) core *.o *.a *.gcda *.$(SHARED_EXT) *.$(SHARED_EXT).* libzstd.pc
	$(RM) dll/libzstd.dll dll/libzstd.lib libzstd-nomt*
	$(RM) -r obj/*
	@echo Cleaning library completed

#-----------------------------------------------------------------------------
# make install is validated only for below listed environments
#-----------------------------------------------------------------------------
ifneq (,$(filter $(UNAME),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS Haiku))

all: libzstd.pc

HAS_EXPLICIT_EXEC_PREFIX := $(if $(or $(EXEC_PREFIX),$(exec_prefix)),1,)

DESTDIR     ?=
# directory variables : GNU conventions prefer lowercase
# see https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html
# support both lower and uppercase (BSD), use uppercase in script
prefix      ?= /usr/local
PREFIX      ?= $(prefix)
exec_prefix ?= $(PREFIX)
EXEC_PREFIX ?= $(exec_prefix)
libdir      ?= $(EXEC_PREFIX)/lib
LIBDIR      ?= $(libdir)
includedir  ?= $(PREFIX)/include
INCLUDEDIR  ?= $(includedir)

PCINCDIR := $(patsubst $(PREFIX)%,%,$(INCLUDEDIR))
PCLIBDIR := $(patsubst $(EXEC_PREFIX)%,%,$(LIBDIR))

# If we successfully stripped off a prefix, we'll add a reference to the
# relevant pc variable.
PCINCPREFIX := $(if $(findstring $(INCLUDEDIR),$(PCINCDIR)),,$${prefix})
PCLIBPREFIX := $(if $(findstring $(LIBDIR),$(PCLIBDIR)),,$${exec_prefix})

# If no explicit EXEC_PREFIX was set by the caller, write it out as a reference
# to PREFIX, rather than as a resolved value.
PCEXEC_PREFIX := $(if $(HAS_EXPLICIT_EXEC_PREFIX),$(EXEC_PREFIX),$${prefix})

ifneq (,$(filter $(UNAME),FreeBSD NetBSD DragonFly))
  PKGCONFIGDIR ?= $(PREFIX)/libdata/pkgconfig
else
  PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
endif

ifneq (,$(filter $(UNAME),SunOS))
  INSTALL ?= ginstall
else
  INSTALL ?= install
endif

INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA    ?= $(INSTALL) -m 644


libzstd.pc:
libzstd.pc: libzstd.pc.in
	@echo creating pkgconfig
	@sed $(SED_ERE_OPT) \
	        -e 's|@PREFIX@|$(PREFIX)|' \
	        -e 's|@EXEC_PREFIX@|$(PCEXEC_PREFIX)|' \
          -e 's|@INCLUDEDIR@|$(PCINCPREFIX)$(PCINCDIR)|' \
          -e 's|@LIBDIR@|$(PCLIBPREFIX)$(PCLIBDIR)|' \
          -e 's|@VERSION@|$(VERSION)|' \
          $< >$@

install: install-pc install-static install-shared install-includes
	@echo zstd static and shared library installed

install-pc: libzstd.pc
	[ -e $(DESTDIR)$(PKGCONFIGDIR) ] || $(INSTALL) -d -m 755 $(DESTDIR)$(PKGCONFIGDIR)/
	$(INSTALL_DATA) libzstd.pc $(DESTDIR)$(PKGCONFIGDIR)/

install-static:
	# only generate libzstd.a if it's not already present
	[ -e libzstd.a ] || $(MAKE) libzstd.a-release
	[ -e $(DESTDIR)$(LIBDIR) ] || $(INSTALL) -d -m 755 $(DESTDIR)$(LIBDIR)/
	@echo Installing static library
	$(INSTALL_DATA) libzstd.a $(DESTDIR)$(LIBDIR)

install-shared:
	# only generate libzstd.so if it's not already present
	[ -e $(LIBZSTD) ] || $(MAKE) libzstd-release
	[ -e $(DESTDIR)$(LIBDIR) ] || $(INSTALL) -d -m 755 $(DESTDIR)$(LIBDIR)/
	@echo Installing shared library
	$(INSTALL_PROGRAM) $(LIBZSTD) $(DESTDIR)$(LIBDIR)
	ln -sf $(LIBZSTD) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR)
	ln -sf $(LIBZSTD) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT)

install-includes:
	[ -e $(DESTDIR)$(INCLUDEDIR) ] || $(INSTALL) -d -m 755 $(DESTDIR)$(INCLUDEDIR)/
	@echo Installing includes
	$(INSTALL_DATA) zstd.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_DATA) common/zstd_errors.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_DATA) dictBuilder/zdict.h $(DESTDIR)$(INCLUDEDIR)

uninstall:
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.a
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT)
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR)
	$(RM) $(DESTDIR)$(LIBDIR)/$(LIBZSTD)
	$(RM) $(DESTDIR)$(PKGCONFIGDIR)/libzstd.pc
	$(RM) $(DESTDIR)$(INCLUDEDIR)/zstd.h
	$(RM) $(DESTDIR)$(INCLUDEDIR)/zstd_errors.h
	$(RM) $(DESTDIR)$(INCLUDEDIR)/zdict.h
	@echo zstd libraries successfully uninstalled

endif
