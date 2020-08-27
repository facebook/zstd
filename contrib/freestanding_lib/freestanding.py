#!/usr/bin/env python3
# ################################################################
# Copyright (c) 2020-2020, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
# ##########################################################################

import argparse
import contextlib
import os
import re
import shutil
import sys
from typing import Optional

INCLUDED_SUBDIRS = ["common", "compress", "decompress"]

SKIPPED_FILES = [
    "common/zstd_deps.h",
    "common/pool.c",
    "common/pool.h",
    "common/threading.c",
    "common/threading.h",
    "compress/zstdmt_compress.h",
    "compress/zstdmt_compress.c",
]

XXHASH_FILES = [
    "common/xxhash.c",
    "common/xxhash.h",
]


class FileLines(object):
    def __init__(self, filename):
        self.filename = filename
        with open(self.filename, "r") as f:
            self.lines = f.readlines()

    def write(self):
        with open(self.filename, "w") as f:
            f.write("".join(self.lines))


class Freestanding(object):
    def __init__(
            self,zstd_deps: str, source_lib: str, output_lib: str,
            external_xxhash: bool, rewritten_includes: [(str, str)],
            defs: [(str, Optional[str])], undefs: [str], excludes: [str]
    ):
        self._zstd_deps = zstd_deps
        self._src_lib = source_lib
        self._dst_lib = output_lib
        self._external_xxhash = external_xxhash
        self._rewritten_includes = rewritten_includes
        self._defs = defs
        self._undefs = undefs
        self._excludes = excludes

    def _dst_lib_file_paths(self):
        """
        Yields all the file paths in the dst_lib.
        """
        for root, dirname, filenames in os.walk(self._dst_lib):
            for filename in filenames:
                filepath = os.path.join(root, filename)
                yield filepath

    def _log(self, *args, **kwargs):
        print(*args, **kwargs)

    def _copy_file(self, lib_path):
        if not (lib_path.endswith(".c") or lib_path.endswith(".h")):
            return
        if lib_path in SKIPPED_FILES:
            self._log(f"\tSkipping file: {lib_path}")
            return
        if self._external_xxhash and lib_path in XXHASH_FILES:
            self._log(f"\tSkipping xxhash file: {lib_path}")
            return

        src_path = os.path.join(self._src_lib, lib_path)
        dst_path = os.path.join(self._dst_lib, lib_path)
        self._log(f"\tCopying: {src_path} -> {dst_path}")
        shutil.copyfile(src_path, dst_path)
    
    def _copy_source_lib(self):
        self._log("Copying source library into output library")

        assert os.path.exists(self._src_lib)
        os.makedirs(self._dst_lib, exist_ok=True)
        self._copy_file("zstd.h")
        for subdir in INCLUDED_SUBDIRS:
            src_dir = os.path.join(self._src_lib, subdir)
            dst_dir = os.path.join(self._dst_lib, subdir)
            
            assert os.path.exists(src_dir)
            os.makedirs(dst_dir, exist_ok=True)

            for filename in os.listdir(src_dir):
                lib_path = os.path.join(subdir, filename)
                self._copy_file(lib_path)
    
    def _copy_zstd_deps(self):
        dst_zstd_deps = os.path.join(self._dst_lib, "common", "zstd_deps.h")
        self._log(f"Copying zstd_deps: {self._zstd_deps} -> {dst_zstd_deps}")
        shutil.copyfile(self._zstd_deps, dst_zstd_deps)

    def _hardwire_preprocessor(self, name: str, value: Optional[str] = None, undef=False):
        """
        If value=None then hardwire that it is defined, but not what the value is.
        If undef=True then value must be None.
        If value='' then the macro is defined to '' exactly.
        """
        assert not (undef and value is not None)
        for filepath in self._dst_lib_file_paths():
            file = FileLines(filepath)
    
    def _hardwire_defines(self):
        self._log("Hardwiring defined macros")
        for (name, value) in self._defs:
            self._log(f"\tHardwiring: #define {name} {value}")
            self._hardwire_preprocessor(name, value=value)
        self._log("Hardwiring undefined macros")
        for name in self._undefs:
            self._log(f"\tHardwiring: #undef {name}")
            self._hardwire_preprocessor(name, undef=True)

    def _remove_excludes(self):
        self._log("Removing excluded sections")
        for exclude in self._excludes:
            self._log(f"\tRemoving excluded sections for: {exclude}")
            begin_re = re.compile(f"BEGIN {exclude}")
            end_re = re.compile(f"END {exclude}")
            for filepath in self._dst_lib_file_paths():
                file = FileLines(filepath)
                outlines = []
                skipped = []
                emit = True
                for line in file.lines:
                    if emit and begin_re.search(line) is not None:
                        assert end_re.search(line) is None
                        emit = False
                    if emit:
                        outlines.append(line)
                    else:
                        skipped.append(line)
                        if end_re.search(line) is not None:
                            assert begin_re.search(line) is None
                            self._log(f"\t\tRemoving excluded section: {exclude}") 
                            for s in skipped:
                                self._log(f"\t\t\t- {s}")
                            emit = True
                            skipped = []
                if not emit:
                    raise RuntimeError("Excluded section unfinished!")
                file.lines = outlines
                file.write()

    def _rewrite_include(self, original, rewritten):
        self._log(f"\tRewriting include: {original} -> {rewritten}")
        regex = re.compile(f"\\s*#\\s*include\\s*(?P<include>{original})")
        for filepath in self._dst_lib_file_paths():
            file = FileLines(filepath)
            for i, line in enumerate(file.lines):
                match = regex.match(line)
                if match is None:
                    continue
                s = match.start('include')
                e = match.end('include')
                file.lines[i] = line[:s] + rewritten + line[e:]
            file.write()
    
    def _rewrite_includes(self):
        self._log("Rewriting includes")
        for original, rewritten in self._rewritten_includes:
            self._rewrite_include(original, rewritten)
    
    def go(self):
        self._copy_source_lib()
        self._copy_zstd_deps()
        self._hardwire_defines()
        self._remove_excludes()
        self._rewrite_includes()


def parse_defines(defines: [str]) -> [(str, Optional[str])]:
    output = []
    for define in defines:
        parsed = define.split('=')
        if len(parsed) == 1:
            output.append((parsed[0], None))
        elif len(parsed) == 2:
            output.append((parsed[0], parsed[1]))
        else:
            raise RuntimeError(f"Bad define: {define}")
    return output


def parse_rewritten_includes(rewritten_includes: [str]) -> [(str, str)]:
    output = []
    for rewritten_include in rewritten_includes:
        parsed = rewritten_include.split('=')
        if len(parsed) == 2:
            output.append((parsed[0], parsed[1]))
        else:
            raise RuntimeError(f"Bad rewritten include: {rewritten_include}")
    return output



def main(name, args):
    parser = argparse.ArgumentParser(prog=name)
    parser.add_argument("--zstd-deps", default="zstd_deps.h", help="Zstd dependencies file")
    parser.add_argument("--source-lib", default="../../lib", help="Location of the zstd library")
    parser.add_argument("--output-lib", default="./freestanding_lib", help="Where to output the freestanding zstd library")
    parser.add_argument("--xxhash", default=None, help="Alternate external xxhash include e.g. --xxhash='<xxhash.h>'. If set xxhash is not included.")
    parser.add_argument("--rewrite-include", default=[], dest="rewritten_includes", action="append", help="Rewrite an include REGEX=NEW (e.g. '<stddef\\.h>=<linux/types.h>')")
    parser.add_argument("-D", "--define", default=[], dest="defs", action="append", help="Pre-define this macro (can be passed multiple times)")
    parser.add_argument("-U", "--undefine", default=[], dest="undefs", action="append", help="Pre-undefine this macro (can be passed mutliple times)")
    parser.add_argument("-E", "--exclude", default=[], dest="excludes", action="append", help="Exclude all lines between 'BEGIN <EXCLUDE>' and 'END <EXCLUDE>'")
    args = parser.parse_args(args)

    # Always remove threading
    if "ZSTD_MULTITHREAD" not in args.undefs:
        args.undefs.append("ZSTD_MULTITHREAD")

    args.defs = parse_defines(args.defs)
    for name, _ in args.defs:
        if name in args.undefs:
            raise RuntimeError(f"{name} is both defined and undefined!")

    args.rewritten_includes = parse_rewritten_includes(args.rewritten_includes)

    external_xxhash = False
    if args.xxhash is not None:
        external_xxhash = True
        args.rewritten_includes.append(('"(\\.\\./common/)?xxhash.h"', args.xxhash))

    print(args.zstd_deps)
    print(args.output_lib)
    print(args.source_lib)
    print(args.xxhash)
    print(args.rewritten_includes)
    print(args.defs)
    print(args.undefs)

    Freestanding(
        args.zstd_deps,
        args.source_lib,
        args.output_lib,
        external_xxhash,
        args.rewritten_includes,
        args.defs,
        args.undefs,
        args.excludes
    ).go()

if __name__ == "__main__":
    main(sys.argv[0], sys.argv[1:])
