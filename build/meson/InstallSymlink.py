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
import pathlib  # since Python 3.4


def install_symlink(src, dst, install_dir, dst_is_dir=False, dir_mode=0o777):
  if not install_dir.exists():
    install_dir.mkdir(mode=dir_mode, parents=True, exist_ok=True)
  if not install_dir.is_dir():
    raise NotADirectoryError(install_dir)

  new_dst = install_dir.joinpath(dst)
  if new_dst.is_symlink() and os.readlink(new_dst) == src:
    print('File exists: {!r} -> {!r}'.format(new_dst, src))
    return
  print('Installing symlink {!r} -> {!r}'.format(new_dst, src))
  new_dst.symlink_to(src, target_is_directory=dst_is_dir)


def main():
  import argparse
  parser = argparse.ArgumentParser(description='Install a symlink',
      usage='InstallSymlink.py [-h] [-d] [-m MODE] src dst install_dir\n\n'
            'example:\n'
            '\tInstallSymlink.py dash sh /bin')
  parser.add_argument('src', help='target to link')
  parser.add_argument('dst', help='link name')
  parser.add_argument('install_dir', help='installation directory')
  parser.add_argument('-d', '--isdir',
      action='store_true',
      help='dst is a directory')
  parser.add_argument('-m', '--mode',
      help='directory mode on creating if not exist',
      default='0o777')
  args = parser.parse_args()

  src = args.src
  dst = args.dst
  dst_is_dir = args.isdir
  dir_mode = int(args.mode, 8)
  install_dir = pathlib.Path(args.install_dir)

  meson_destdir = os.environ.get('MESON_INSTALL_DESTDIR_PREFIX')
  if meson_destdir:
    install_dir = pathlib.Path(meson_destdir).joinpath(install_dir)
  install_symlink(src, dst, install_dir, dst_is_dir, dir_mode)


if __name__ == '__main__':
  main()
