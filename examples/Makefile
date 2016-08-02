# ##########################################################################
# ZSTD educational examples - Makefile
# Copyright (C) Yann Collet 2016
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
#  - zstd homepage : http://www.zstd.net/
# ##########################################################################

# This Makefile presumes libzstd is installed, using `sudo make install`

LDFLAGS+= -lzstd

.PHONY: default all clean test

default: all

all: simple_compression simple_decompression \
	dictionary_compression dictionary_decompression

simple_compression : simple_compression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

simple_decompression : simple_decompression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

dictionary_compression : dictionary_compression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

dictionary_decompression : dictionary_decompression.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	@rm -f core *.o tmp* result* *.zst \
        simple_compression simple_decompression \
		dictionary_compression dictionary_decompression
	@echo Cleaning completed

test: all
	cp README.md tmp
	./simple_compression tmp
	@echo starting simple_decompression
	./simple_decompression tmp.zst
	@echo dictionary compression
	./dictionary_compression tmp README.md
	@echo dictionary decompression
	./dictionary_decompression tmp.zst README.md
	@echo tests completed
