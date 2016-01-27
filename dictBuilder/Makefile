# ##########################################################################
# Dict Builder - Makefile
# Copyright (C) Yann Collet 2015
#
# GPL v2 License
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# You can contact the author at :
#  - ZSTD source repository : http://code.google.com/p/zstd/
#  - Public forum : https://groups.google.com/forum/#!forum/lz4c
# ##########################################################################

CPPFLAGS= -I../lib
CFLAGS ?= -O3  
CFLAGS += -std=c99 -Wall -Wextra -Wshadow -Wcast-qual -Wcast-align -Wundef -Wstrict-prototypes -Wstrict-aliasing=1
FLAGS   = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(MOREFLAGS)

ZSTDDIR = ../lib


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
VOID = nul
else
EXT =
VOID = /dev/null
endif


.PHONY: default all test

default: dictBuilder

all: dictBuilder

dictBuilder: dictBuilder.c dibcli.c divsufsort.c sssort.c trsort.c $(ZSTDDIR)/huff0.c $(ZSTDDIR)/fse.c $(ZSTDDIR)/zstd_decompress.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

clean:
	@rm -f core *.o tmp* result* *.gcda \
        dictBuilder$(EXT)
	@echo Cleaning completed

test: clean dictBuilder

clangtest: CC = clang
clangtest: CFLAGS += -Werror
clangtest: clean dictBuilder

gpptest: CC = g++
gpptest: CFLAGS=-O3 -Wall -Wextra -Wshadow -Wcast-align -Wcast-qual -Wundef -Werror
gpptest: clean dictBuilder

