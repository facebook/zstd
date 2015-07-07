# ##########################################################################
# ZSTD programs - Makefile
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
# zstd : Command Line Utility, supporting gzip-like arguments
# datagen : Synthetic and parametrable data generator, for tests
# fuzzer  : Test tool, to check zstd integrity on target platform
# fuzzer32: Same as fuzzer, but forced to compile in 32-bits mode
# fullbench  : Precisely measure speed for each zstd inner function
# fullbench32: Same as fullbench, but forced to compile in 32-bits mode
# ##########################################################################

RELEASE?= v0.0.2

DESTDIR?=
PREFIX ?= /usr
CFLAGS ?= -O3
CFLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-qual -Wcast-align -Wstrict-prototypes -DZSTD_VERSION=\"$(RELEASE)\"
FLAGS   = -I../lib $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1
ZSTDDIR=../lib

TEST_FILES = COPYING
TEST_TARGETS=test-native


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
VOID = nul
else
EXT =
VOID = /dev/null
endif


default: zstd

all: zstd zstd32 fullbench fullbench32 fuzzer fuzzer32

zstd: $(ZSTDDIR)/zstd.c xxhash.c bench.c fileio.c zstdcli.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

zstd32: $(ZSTDDIR)/zstd.c xxhash.c bench.c fileio.c zstdcli.c
	$(CC) -m32 $(FLAGS) $^ -o $@$(EXT)

fullbench  : $(ZSTDDIR)/zstd.c datagen.c fullbench.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

fullbench32: $(ZSTDDIR)/zstd.c datagen.c fullbench.c
	$(CC) -m32 $(FLAGS) $^ -o $@$(EXT)

fuzzer  : $(ZSTDDIR)/zstd.c xxhash.c fuzzer.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

fuzzer32: $(ZSTDDIR)/zstd.c xxhash.c fuzzer.c
	$(CC) -m32 $(FLAGS) $^ -o $@$(EXT)

datagen : datagen.c datagencli.c
	$(CC)      $(FLAGS) $^ -o $@$(EXT)

clean:
	@rm -f core *.o tmp \
        zstd$(EXT) zstd32$(EXT) \
        fullbench$(EXT) fullbench32$(EXT) \
        fuzzer$(EXT) fuzzer32$(EXT) \
	datagen$(EXT)
	@echo Cleaning completed


#------------------------------------------------------------------------
#make install is validated only for Linux, OSX, kFreeBSD and Hurd targets
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU))

install: zstd
	@echo Installing binaries
	@install -d -m 755 $(DESTDIR)$(BINDIR)/ $(DESTDIR)$(MANDIR)/
	@install -m 755 zstd$(EXT) $(DESTDIR)$(BINDIR)/zstd$(EXT)
	@ln -sf zstd$(EXT) $(DESTDIR)$(BINDIR)/zstdcat
	@ln -sf zstd$(EXT) $(DESTDIR)$(BINDIR)/unzstd
	@echo Installing man pages
	@install -m 644 zstd.1 $(DESTDIR)$(MANDIR)/zstd.1
	@install -m 644 zstdcat.1 $(DESTDIR)$(MANDIR)/zstdcat.1
	@install -m 644 unzstd.1 $(DESTDIR)$(MANDIR)/unzstd.1
	@echo zstd installation completed

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/zstdcat
	rm -f $(DESTDIR)$(BINDIR)/unzstd
	[ -x $(DESTDIR)$(BINDIR)/zstd$(EXT) ] && rm -f $(DESTDIR)$(BINDIR)/zstd$(EXT)
	[ -f $(DESTDIR)$(MANDIR)/zstd.1 ] && rm -f $(DESTDIR)$(MANDIR)/zstd.1
	[ -f $(DESTDIR)$(MANDIR)/zstdcat.1 ] && rm -f $(DESTDIR)$(MANDIR)/zstdcat.1
	[ -f $(DESTDIR)$(MANDIR)/unzstd.1 ] && rm -f $(DESTDIR)$(MANDIR)/unzstd.1
	@echo zstd programs successfully uninstalled

test: test-zstd test-fullbench test-fuzzer test-mem

test32: test-zstd32 test-fullbench32 test-fuzzer32

test-all: test test32

test-zstd: zstd datagen
	./datagen          | ./zstd -v    | ./zstd -d > $(VOID)
	./datagen -g256MB  | ./zstd -v    | ./zstd -d > $(VOID)
	./datagen -g6GB -P99 | ./zstd -vq | ./zstd -d > $(VOID)

test-zstd32: zstd32 datagen
	./datagen          | ./zstd32 -v  | ./zstd32 -d > $(VOID)
	./datagen -g256MB  | ./zstd32 -v  | ./zstd32 -d > $(VOID)
	./datagen -g6GB -P99 | ./zstd32 -vq | ./zstd32 -d > $(VOID)

test-fullbench: fullbench datagen
	./fullbench -i1
	./fullbench -i1 -P0

test-fullbench32: fullbench32 datagen
	./fullbench32 -i1
	./fullbench32 -i1 -P0

test-fuzzer: fuzzer
	./fuzzer

test-fuzzer32: fuzzer32
	./fuzzer32

test-mem: zstd datagen fuzzer fullbench
	@echo "\n ---- valgrind tests : memory analyzer ----"
	valgrind --leak-check=yes --error-exitcode=1 ./datagen -g50M > /dev/null
	./datagen -g16KB > tmp
	valgrind --leak-check=yes --error-exitcode=1 ./zstd -vf tmp /dev/null
	./datagen -g128MB > tmp
	valgrind --leak-check=yes --error-exitcode=1 ./zstd -vf tmp /dev/null
	@rm tmp
	valgrind --leak-check=yes --error-exitcode=1 ./fuzzer -i128 -t1
	valgrind --leak-check=yes --error-exitcode=1 ./fullbench -i1

endif
