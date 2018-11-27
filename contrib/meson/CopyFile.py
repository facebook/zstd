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
import shutil


def usage():
    print('usage: python3 CreateSymlink.py <src> <dst>')
    print('Copy the file named src to a file named dst')
    sys.exit(1)


def main():
    if len(sys.argv) < 3:
        usage()
    src = sys.argv[1]
    dst = sys.argv[2]

    if os.path.exists(dst):
            print ('File already exists: %r' % (dst))
            return

    shutil.copy2(src, dst)


if __name__ == '__main__':
    main()
