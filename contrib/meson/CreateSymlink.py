#!/usr/bin/env python3
# #############################################################################
# Copyright (c) 2018-present    lzutao <taolzu(at)gmail.com>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################
import os
import sys


def usage():
    print('usage: python3 CreateSymlink.py <src> <dst> [dst is dir: True or False]')
    sys.exit(1)


def main():
    if len(sys.argv) < 3:
        usage()
    src = sys.argv[1]
    dst = sys.argv[2]
    is_dir = False
    if len(sys.argv) == 4:
        is_dir = bool(sys.argv[3])

    if os.path.islink(dst) and os.readlink(dst) == src:
            print ('File exists: %r -> %r' % (dst, src))
            return

    os.symlink(src, dst, is_dir)


if __name__ == '__main__':
    main()
