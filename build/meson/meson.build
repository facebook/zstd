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
  license: ['BSD', 'GPLv2'],
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
windows_mod = import('windows')

host_machine_os = host_machine.system()
os_windows = 'windows'
os_linux = 'linux'
os_darwin = 'darwin'
os_freebsd = 'freebsd'
os_sun = 'sunos'

cc_id = cc.get_id()
compiler_gcc = 'gcc'
compiler_clang = 'clang'
compiler_msvc = 'msvc'

zstd_version = meson.project_version()
zstd_libversion = ''

# =============================================================================
# Project directories
# =============================================================================

zstd_rootdir = '../..'

# =============================================================================
# Installation directories
# =============================================================================

if host_machine_os == os_windows
  zstd_prefix = '.'
  zstd_bindir = 'bin'
  zstd_datadir = 'share'
  zstd_mandir = join_paths(zstd_datadir, 'man')
else
  zstd_prefix = get_option('prefix')
  zstd_bindir = join_paths(zstd_prefix, get_option('bindir'))
  zstd_datadir = join_paths(zstd_prefix, get_option('datadir'))
  zstd_mandir = join_paths(zstd_prefix, get_option('mandir'))
endif

zstd_docdir = join_paths(zstd_datadir, 'doc', meson.project_name())

# =============================================================================
# Project options
# =============================================================================

enable_debug = get_option('debug')
default_library_type = get_option('default_library')
meson_buildtype = get_option('buildtype')
with_legacy_support = get_option('with-legacy-support')
with_programs = get_option('with-programs')
with_tests = get_option('with-tests')
with_contrib = get_option('with-contrib')
with_debug_level = get_option('with-debug-level')
enable_multithread = get_option('enable-multithread')
enable_static_runtime = get_option('enable-static-runtime')
enable_zlib = get_option('enable-zlib')
enable_lzma = get_option('enable-lzma')
enable_lz4 = get_option('enable-lz4')
enable_backtrace = get_option('enable-backtrace')

# =============================================================================
# Helper scripts for Meson
# =============================================================================

GetZstdLibraryVersion_py = files('GetZstdLibraryVersion.py')

# =============================================================================
# Getting project version from zstd.h
# =============================================================================

zstd_h_file = join_paths(meson.current_source_dir(), zstd_rootdir, 'lib/zstd.h')
r = run_command(python3, GetZstdLibraryVersion_py, zstd_h_file)
if r.returncode() == 0
  output = r.stdout().strip()
  if output.version_compare('>@0@'.format(zstd_version))
    zstd_version = output
    message('Project version is now: @0@'.format(zstd_version))
  endif
endif

if host_machine_os != os_windows
  zstd_libversion = zstd_version
endif

# =============================================================================
# Dependencies
# =============================================================================

libm_dep = cc.find_library('m', required: with_tests)
thread_dep = dependency('threads', required: enable_multithread)
# Arguments in dependency should be equivalent to those passed to pkg-config
zlib_dep = dependency('zlib', required: enable_zlib)
lzma_dep = dependency('liblzma', required: enable_lzma)
lz4_dep = dependency('liblz4', required: enable_lz4)

# =============================================================================
# Compiler flags
# =============================================================================

add_project_arguments('-DXXH_NAMESPACE=ZSTD_', language: ['c'])

if [compiler_gcc, compiler_clang].contains(cc_id)
  common_warning_flags = [ '-Wextra', '-Wundef', '-Wshadow', '-Wcast-align', '-Wcast-qual' ]
  if cc_id == compiler_clang
    common_warning_flags += ['-Werror', '-Wconversion', '-Wno-sign-conversion', '-Wdocumentation']
  endif
  cc_compile_flags = cc.get_supported_arguments(common_warning_flags + ['-Wstrict-prototypes'])
  cxx_compile_flags = cxx.get_supported_arguments(common_warning_flags)
  add_project_arguments(cc_compile_flags, language : 'c')
  add_project_arguments(cxx_compile_flags, language : 'cpp')
elif cc_id == compiler_msvc
  msvc_compile_flags = [ '/D_UNICODE', '/DUNICODE' ]
  if enable_multithread
    msvc_compile_flags += '/MP'
  endif
  if enable_static_runtime
    msvc_compile_flags += '/MT'
  endif
  add_project_arguments(msvc_compile_flags, language: ['c', 'cpp'])
endif

# =============================================================================
# Subdirs
# =============================================================================

subdir('lib')

if with_programs
  subdir('programs')
endif

if with_tests
  subdir('tests')
endif

if with_contrib
  subdir('contrib')
endif
