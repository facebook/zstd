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
programs_dir = join_paths(zstd_source_dir, 'programs')

zstdcli_c_file = join_paths(programs_dir, 'zstdcli.c')
util_c_file = join_paths(programs_dir, 'util.c')
fileio_c_file = join_paths(programs_dir, 'fileio.c')
zstd_programs_sources = [zstdcli_c_file,
    util_c_file,
    fileio_c_file,
    join_paths(programs_dir, 'benchfn.c'),
    join_paths(programs_dir, 'benchzstd.c'),
    join_paths(programs_dir, 'datagen.c'),
    join_paths(programs_dir, 'dibio.c')]

zstd_c_args = []
if enable_multithread
    zstd_c_args += [ '-DZSTD_MULTITHREAD' ]
endif

zstd_deps = []
if enable_zlib and zlib_dep.found()
    zstd_deps += [ zlib_dep ]
    zstd_c_args += [ '-DZSTD_GZCOMPRESS', '-DZSTD_GZDECOMPRESS' ]
endif

if enable_lzma and lzma_dep.found()
    zstd_deps += [ lzma_dep ]
    zstd_c_args += [ '-DZSTD_LZMACOMPRESS', '-DZSTD_LZMADECOMPRESS' ]
endif

zstd = executable('zstd',
    zstd_programs_sources,
    c_args: zstd_c_args,
    include_directories: libzstd_includes,
    link_with: libzstd,
    dependencies: zstd_deps,
    install: true)

zstd_frugal_sources = [join_paths(programs_dir, 'zstdcli.c'),
    util_c_file,
    fileio_c_file]

executable('zstd-frugal',
    zstd_frugal_sources,
    include_directories: libzstd_includes,
    link_with: libzstd,
    c_args: [ '-DZSTD_NOBENCH', '-DZSTD_NODICT' ],
    install: true)

# =============================================================================
# Program symlinks
# =============================================================================

InstallSymlink_py = join_paths('..', 'InstallSymlink.py')
meson.add_install_script(InstallSymlink_py, 'zstd', 'zstdcat', zstd_bindir)
meson.add_install_script(InstallSymlink_py, 'zstd', 'unzstd', zstd_bindir)

if enable_multithread
    meson.add_install_script(InstallSymlink_py, 'zstd', 'zstdmt', zstd_bindir)
endif

# =============================================================================
# Manpages
# =============================================================================

zstd_man1_dir = join_paths(zstd_mandir, 'man1')

install_man(join_paths(programs_dir, 'zstd.1'),
    join_paths(programs_dir, 'zstdgrep.1'),
    join_paths(programs_dir, 'zstdless.1'))

# Meson automatically compresses manpages to gz format
# WARNING: This may fail on Windows. Test NEEDED.
meson.add_install_script(InstallSymlink_py, 'zstd.1.gz', 'zstdcat.1.gz', zstd_man1_dir)
meson.add_install_script(InstallSymlink_py, 'zstd.1.gz', 'unzstd.1.gz', zstd_man1_dir)

if enable_multithread
    meson.add_install_script(InstallSymlink_py, 'zstd.1.gz', 'zstdmt.1.gz', zstd_man1_dir)
endif
