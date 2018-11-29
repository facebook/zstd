# #############################################################################
# Copyright (c) 2018-present     Dima Krasner <dima@dimakrasner.com>
#                                lzutao <taolzu(at)gmail.com>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################

zstd_source_dir = join_paths('..', '..', '..')
library_dir = join_paths(zstd_source_dir, 'lib')
library_common_dir = join_paths(library_dir, 'common')
library_compress_dir = join_paths(library_dir, 'compress')
library_decompress_dir = join_paths(library_dir, 'decompress')
library_dictbuilder_dir = join_paths(library_dir, 'dictBuilder')
library_deprecated_dir = join_paths(library_dir, 'deprecated')
library_legacy_dir = join_paths(library_dir, 'legacy')

libzstd_includes = [include_directories(library_dir,
    library_common_dir,
    library_compress_dir,
    library_decompress_dir,
    library_dictbuilder_dir,
    library_deprecated_dir)]

libzstd_sources = [join_paths(library_common_dir, 'entropy_common.c'),
    join_paths(library_common_dir, 'fse_decompress.c'),
    join_paths(library_common_dir, 'threading.c'),
    join_paths(library_common_dir, 'pool.c'),
    join_paths(library_common_dir, 'zstd_common.c'),
    join_paths(library_common_dir, 'error_private.c'),
    join_paths(library_common_dir, 'xxhash.c'),
    join_paths(library_compress_dir, 'hist.c'),
    join_paths(library_compress_dir, 'fse_compress.c'),
    join_paths(library_compress_dir, 'huf_compress.c'),
    join_paths(library_compress_dir, 'zstd_compress.c'),
    join_paths(library_compress_dir, 'zstdmt_compress.c'),
    join_paths(library_compress_dir, 'zstd_fast.c'),
    join_paths(library_compress_dir, 'zstd_double_fast.c'),
    join_paths(library_compress_dir, 'zstd_lazy.c'),
    join_paths(library_compress_dir, 'zstd_opt.c'),
    join_paths(library_compress_dir, 'zstd_ldm.c'),
    join_paths(library_decompress_dir, 'huf_decompress.c'),
    join_paths(library_decompress_dir, 'zstd_decompress.c'),
    join_paths(library_decompress_dir, 'zstd_decompress_block.c'),
    join_paths(library_decompress_dir, 'zstd_ddict.c'),
    join_paths(library_dictbuilder_dir, 'cover.c'),
    join_paths(library_dictbuilder_dir, 'fastcover.c'),
    join_paths(library_dictbuilder_dir, 'divsufsort.c'),
    join_paths(library_dictbuilder_dir, 'zdict.c'),
    join_paths(library_deprecated_dir, 'zbuff_common.c'),
    join_paths(library_deprecated_dir, 'zbuff_compress.c'),
    join_paths(library_deprecated_dir, 'zbuff_decompress.c')]

if legacy_support == '0'
    legacy_support = 'false'
endif
if legacy_support != 'false'
    if legacy_support == 'true'
        legacy_support = '5'
    endif
    legacy_int = legacy_support.to_int()
    if legacy_int < 0 or legacy_int >= 8
        legacy_int = 0
    endif
    add_project_arguments('-DZSTD_LEGACY_SUPPORT=@0@'.format(legacy_int),
        language: 'c')
    libzstd_includes += [ include_directories(library_legacy_dir) ]
    # See ZSTD_LEGACY_SUPPORT of lib/README.md
    message('Enable legacy support back to version 0.@0@'.format(legacy_int))
    if legacy_int <= 1
        libzstd_sources += join_paths(library_legacy_dir, 'zstd_v01.c')
    endif
    if legacy_int <= 2
        libzstd_sources += join_paths(library_legacy_dir, 'zstd_v02.c')
    endif
    if legacy_int <= 3
        libzstd_sources += join_paths(library_legacy_dir, 'zstd_v03.c')
    endif
    if legacy_int <= 4
        libzstd_sources += join_paths(library_legacy_dir, 'zstd_v04.c')
    endif
    if legacy_int <= 5
        libzstd_sources += join_paths(library_legacy_dir, 'zstd_v05.c')
    endif
    if legacy_int <= 6
        libzstd_sources += join_paths(library_legacy_dir, 'zstd_v06.c')
    endif
    if legacy_int <= 7
        libzstd_sources += join_paths(library_legacy_dir, 'zstd_v07.c')
    endif
endif

if enable_multithread
    message('Enable multi-threading support')
    add_project_arguments('-DZSTD_MULTITHREAD', language: 'c')
    libzstd_deps = [ thread_dep ]
else
    libzstd_deps = []
endif

libzstd = library('zstd',
    libzstd_sources,
    include_directories: libzstd_includes,
    dependencies: libzstd_deps,
    install: true,
    soversion: zstd_version)

pkgconfig.generate(name: 'libzstd',
    filebase: 'libzstd',
    libraries: [libzstd],
    description: 'fast lossless compression algorithm library',
    version: zstd_version,
    url: 'http://www.zstd.net/')

install_headers(join_paths(library_dir, 'zstd.h'),
    join_paths(library_deprecated_dir, 'zbuff.h'),
    join_paths(library_dictbuilder_dir, 'zdict.h'),
    join_paths(library_common_dir, 'zstd_errors.h'))
