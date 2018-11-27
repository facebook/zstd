# #############################################################################
# Copyright (c) 2018-present    lzutao <taolzu(at)gmail.com>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################

zstd_source_dir = join_paths('..', '..', '..', '..')
library_dir = join_paths(zstd_source_dir, 'lib')
library_common_dir = join_paths(library_dir, 'common')
programs_dir = join_paths(zstd_source_dir, 'programs')
contrib_pzstd_dir = join_paths(zstd_source_dir, 'contrib', 'pzstd')

pzstd_includes = include_directories(programs_dir,
    library_dir,
    library_common_dir,
    contrib_pzstd_dir)
pzstd_sources = [join_paths(programs_dir, 'util.c'),
    join_paths(contrib_pzstd_dir, 'main.cpp'),
    join_paths(contrib_pzstd_dir, 'Options.cpp'),
    join_paths(contrib_pzstd_dir, 'Pzstd.cpp'),
    join_paths(contrib_pzstd_dir, 'SkippableFrame.cpp')]

pzstd = executable('pzstd',
    pzstd_sources,
    cpp_args: [ '-DNDEBUG', '-Wno-shadow' ],
    include_directories: pzstd_includes,
    link_with: libzstd,
    dependencies: [ thread_dep ],
    install: true)
