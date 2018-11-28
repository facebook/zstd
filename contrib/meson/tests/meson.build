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
tests_dir = join_paths(zstd_source_dir, 'tests')

datagen_c_file = join_paths(programs_dir, 'datagen.c')
util_c_file = join_paths(programs_dir, 'util.c')
benchfn_c_file = join_paths(programs_dir, 'benchfn.c')

datagen_sources = [datagen_c_file,
    join_paths(tests_dir, 'datagencli.c')]
test_includes = libzstd_includes + [include_directories(programs_dir)]

datagen = executable('datagen',
    datagen_sources,
    include_directories: test_includes,
    link_with: libzstd,
    install: false)
test('datagen', datagen)

fullbench_sources = [datagen_c_file,
    util_c_file,
    benchfn_c_file,
    join_paths(programs_dir, 'benchzstd.c'),
    join_paths(tests_dir, 'fullbench.c')]
fullbench = executable('fullbench',
    fullbench_sources,
    include_directories: test_includes,
    link_with: libzstd,
    install: false)
test('fullbench', fullbench)

fuzzer_sources = [datagen_c_file,
    util_c_file,
    join_paths(tests_dir, 'fuzzer.c')]
fuzzer = executable('fuzzer',
    fuzzer_sources,
    include_directories: test_includes,
    link_with: libzstd,
    install: false)
test('fuzzer', fuzzer)

paramgrill_sources = [benchfn_c_file,
    join_paths(programs_dir, 'benchzstd.c'),
    datagen_c_file,
    util_c_file,
    join_paths(tests_dir, 'paramgrill.c')]
if host_machine.system() != 'windows'
    paramgrill = executable('paramgrill',
        paramgrill_sources,
        include_directories: test_includes,
        link_with: libzstd,
        dependencies: libm_dep,
        install: false)
    test('paramgrill', paramgrill)
endif
