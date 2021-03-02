# ################################################################
# Copyright (c) 2015-2021, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
# ################################################################

.PHONY: libzstd
libzstd:
	rm -rf linux
	mkdir -p linux
	mkdir -p linux/include/linux
	mkdir -p linux/lib/zstd
	../freestanding_lib/freestanding.py \
		--source-lib ../../lib \
		--output-lib linux/lib/zstd \
		--xxhash '<linux/xxhash.h>' \
		--xxh64-state 'struct xxh64_state' \
		--xxh64-prefix 'xxh64' \
		--rewrite-include '<limits\.h>=<linux/limits.h>' \
		--rewrite-include '<stddef\.h>=<linux/types.h>' \
		-DZSTD_NO_INTRINSICS \
		-DZSTD_NO_UNUSED_FUNCTIONS \
		-DZSTD_LEGACY_SUPPORT=0 \
		-DZSTD_STATIC_LINKING_ONLY \
		-DFSE_STATIC_LINKING_ONLY \
		-DHUF_STATIC_LINKING_ONLY \
		-DXXH_STATIC_LINKING_ONLY \
		-DMEM_FORCE_MEMORY_ACCESS=0 \
		-D__GNUC__ \
		-DSTATIC_BMI2=0 \
		-DZSTD_ADDRESS_SANITIZER=0 \
		-DZSTD_MEMORY_SANITIZER=0 \
		-DZSTD_COMPRESS_HEAPMODE=1 \
		-UZSTD_NO_INLINE \
		-UNO_PREFETCH \
		-U__cplusplus \
		-UZSTD_DLL_EXPORT \
		-UZSTD_DLL_IMPORT \
		-U__ICCARM__ \
		-UZSTD_MULTITHREAD \
		-U_MSC_VER \
		-U_WIN32 \
		-RZSTDLIB_VISIBILITY= \
		-RZSTDERRORLIB_VISIBILITY= \
		-DZSTD_HAVE_WEAK_SYMBOLS=0 \
		-DZSTD_TRACE=0
	cp linux_zstd.h linux/include/linux/zstd.h
	cp zstd_compress_module.c linux/lib/zstd
	cp zstd_decompress_module.c linux/lib/zstd
	cp decompress_sources.h linux/lib/zstd
	cp linux.mk linux/lib/zstd/Makefile

LINUX ?= $(HOME)/repos/linux

.PHONY: import
import: libzstd
	rm -f $(LINUX)/include/linux/zstd.h
	rm -f $(LINUX)/include/linux/zstd_errors.h
	rm -rf $(LINUX)/lib/zstd
	cp linux/include/linux/zstd.h $(LINUX)/include/linux
	cp -r linux/lib/zstd $(LINUX)/lib

import-upstream:
	rm -rf $(LINUX)/lib/zstd
	mkdir $(LINUX)/lib/zstd
	cp ../../lib/zstd.h $(LINUX)/lib/zstd
	cp -r ../../lib/common $(LINUX)/lib/zstd
	cp -r ../../lib/compress $(LINUX)/lib/zstd
	cp -r ../../lib/decompress $(LINUX)/lib/zstd
	rm $(LINUX)/lib/zstd/common/threading.*
	rm $(LINUX)/lib/zstd/common/pool.*
	rm $(LINUX)/lib/zstd/common/xxhash.*
	rm $(LINUX)/lib/zstd/compress/zstdmt_*

.PHONY: test
test: libzstd
	$(MAKE) -C test run-test CFLAGS="-O3 $(CFLAGS)" -j

.PHONY: clean
clean:
	$(RM) -rf linux
