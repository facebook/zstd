# #############################################################################
# Copyright (c) 2018-present     Dima Krasner <dima@dimakrasner.com>
#                                lzutao <taolzu(at)gmail.com>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################

project('zstd',
    ['c', 'cpp'],
    license: 'BSD',
    default_options : ['c_std=c99',
        'cpp_std=c++11',
        'buildtype=release'],
    version: '1.3.8',
    # for install_man
    meson_version: '>=0.47.0')

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')
pkgconfig = import('pkgconfig')
python3 = import('python').find_installation()

zstd_version = meson.project_version()

# =============================================================================
# Project directories
# =============================================================================

zstd_prefix = get_option('prefix')
zstd_bindir = join_paths(zstd_prefix, get_option('bindir'))
zstd_datadir = join_paths(zstd_prefix, get_option('datadir'))
zstd_docdir = join_paths(zstd_datadir, 'doc', meson.project_name())
zstd_mandir = join_paths(zstd_prefix, get_option('mandir'))

zstd_source_dir = join_paths('..', '..')
library_dir = join_paths(zstd_source_dir, 'lib')
contrib_meson_dir = join_paths(zstd_source_dir, 'contrib', 'meson')

# =============================================================================
# Project options
# =============================================================================
enable_debug = get_option('debug')
legacy_support = get_option('legacy_support')
enable_programs = get_option('build_programs')
enable_tests = get_option('build_tests')
enable_contrib = get_option('build_contrib')
enable_multithread = get_option('multithread_support')
enable_zlib = get_option('zlib_support')
enable_lzma = get_option('lzma_support')
enable_lz4 = get_option('lz4_support')

# =============================================================================
# Helper scripts for Meson
# =============================================================================

GetZstdLibraryVersion_py = files('GetZstdLibraryVersion.py')

# =============================================================================
# Getting project version from zstd.h
# =============================================================================

zstd_h_file = join_paths(meson.current_source_dir(), library_dir, 'zstd.h')
r = run_command(python3, GetZstdLibraryVersion_py, zstd_h_file)
if r.returncode() == 0
    output = r.stdout().strip()
    if output.version_compare('>@0@'.format(zstd_version))
        zstd_version = output
        message('Project version is now: @0@'.format(zstd_version))
    endif
endif

# =============================================================================
# Dependencies
# =============================================================================

libm_dep = cc.find_library('m', required: true)
thread_dep = dependency('threads', required: false)
zlib_dep = dependency('zlib', required: false)
lzma_dep = dependency('liblzma', required: false)
lz4_dep = dependency('liblz4', required: false)

# =============================================================================
# Compiler flags
# =============================================================================

if cxx.get_id() == 'gcc' or cxx.get_id() == 'clang'
    common_flags = [ '-DXXH_NAMESPACE=ZSTD_' ]
    common_warning_flags = [ '-Wextra', '-Wundef', '-Wshadow', '-Wcast-align', '-Wcast-qual' ]
    cc_compilation_flags = cc.get_supported_arguments(common_warning_flags)
    cc_compilation_flags += cc.get_supported_arguments(['-Wstrict-prototypes'])
    cc_compilation_flags += common_flags
    cxx_compilation_flags = cxx.get_supported_arguments(common_warning_flags)
    cxx_compilation_flags += common_flags
    add_project_arguments(cc_compilation_flags, language : 'c')
    add_project_arguments(cxx_compilation_flags, language : 'cpp')
endif

# =============================================================================
# Subdirs
# =============================================================================

subdir('lib')

if enable_programs
    subdir('programs')
endif

if enable_tests
    subdir('tests')
endif

if enable_contrib
    subdir('contrib')
endif
