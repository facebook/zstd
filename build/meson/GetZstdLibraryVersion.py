#!/usr/bin/env python3
# #############################################################################
# Copyright (c) 2018-present    lzutao <taolzu(at)gmail.com>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################
import re
import sys


def usage():
    print('usage: python3 GetZstdLibraryVersion.py <path/to/zstd.h>')
    sys.exit(1)


def find_version(filepath):
    version_file_data = None
    with open(filepath) as fd:
        version_file_data = fd.read()

    patterns = r"""#\s*define\s+ZSTD_VERSION_MAJOR\s+([0-9]+)
#\s*define\s+ZSTD_VERSION_MINOR\s+([0-9]+)
#\s*define\s+ZSTD_VERSION_RELEASE\s+([0-9]+)
"""
    regex = re.compile(patterns, re.MULTILINE)
    version_match = regex.search(version_file_data)
    if version_match:
        return version_match.groups()
    raise RuntimeError("Unable to find version string.")


def main():
    if len(sys.argv) < 2:
        usage()

    filepath = sys.argv[1]
    version_tup = find_version(filepath)
    print('.'.join(version_tup))


if __name__ == '__main__':
    main()
