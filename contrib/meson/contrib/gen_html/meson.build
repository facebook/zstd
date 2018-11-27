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
contrib_gen_html_dir = join_paths(zstd_source_dir, 'contrib', 'gen_html')

gen_html_includes = include_directories(programs_dir,
    library_dir,
    library_common_dir,
    contrib_gen_html_dir)

gen_html = executable('gen_html',
    join_paths(contrib_gen_html_dir, 'gen_html.cpp'),
    include_directories: gen_html_includes,
    install: false )

# Update zstd manual
zstd_manual_html = custom_target('zstd_manual.html',
    output : 'zstd_manual.html',
    command : [
        gen_html,
        zstd_version,
        join_paths(meson.current_source_dir(), library_dir, 'zstd.h'),
        '@OUTPUT@'
    ],
    install : true,
    install_dir : zstd_docdir )
