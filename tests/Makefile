# ##########################################################################
# Zstd tests - Makefile
# Copyright (C) Yann Collet 2015-2016
# All rights reserved.
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
#  - ZSTD homepage : http://www.zstd.net/
# ##########################################################################
# versionstest : Compatibility test between zstd versions stored on Github (v0.1+)
# ##########################################################################

PYTHON ?= python3
TESTARTEFACT := versionsTest namespaceTest

.PHONY: default all clean namespaceTest versionsTest

default: all

all: namespaceTest versionsTest

namespaceTest:
	if $(CC) namespaceTest.c ../lib/common/xxhash.c -o $@ ; then echo compilation should fail; exit 1 ; fi
	$(RM) $@

versionsTest:
	$(PYTHON) test-zstd-versions.py

clean:
	@$(RM) -fR $(TESTARTEFACT)
	@echo Cleaning completed
