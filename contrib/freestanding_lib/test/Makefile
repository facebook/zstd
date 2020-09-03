# ################################################################
# Copyright (c) 2015-2020, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
# ################################################################

all: run-test

.PHONY: libzstd
libzstd: clean
	../freestanding.py --source-lib ../../../lib --output-lib ./libzstd


test: libzstd
	$(CC) $(CFLAGS) $(CPPFLAGS) -ffreestanding $(wildcard libzstd/*/*.c) -c
	$(CC) $(CFLAGS) $(CPPFLAGS) -ffreestanding -c test.c
	$(CC) $(LDFLAGS) -ffreestanding -nostdlib *.o -o test

run-test: test
	./test

clean:
	rm -rf ./libzstd/ *.o test
