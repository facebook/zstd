#!/usr/bin/env python3
# #############################################################################
# Copyright (c) 2018-present    lzutao <taolzu(at)gmail.com>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################
import errno
import os


def mkdir_p(path, dir_mode=0o777):
  try:
    os.makedirs(path, mode=dir_mode)
  except OSError as exc:  # Python >2.5
    if exc.errno == errno.EEXIST and os.path.isdir(path):
      pass
    else:
      raise


def InstallSymlink(src, dst, install_dir, dst_is_dir=False, dir_mode=0o777):
  if not os.path.exists(install_dir):
      mkdir_p(install_dir, dir_mode)
  if not os.path.isdir(install_dir):
      raise NotADirectoryError(install_dir)

  new_dst = os.path.join(install_dir, dst)
  if os.path.islink(new_dst) and os.readlink(new_dst) == src:
    print('File exists: %r -> %r' % (dst, src))
    return
  print('Installing symlink %r -> %r' % (new_dst, src))
  os.symlink(src, new_dst, dst_is_dir)


def main():
  import argparse
  parser = argparse.ArgumentParser(description='Install a symlink.\n',
      usage='usage: InstallSymlink.py [-h] [-d] [-m MODE] src dst '
            'install_dir\n\n'
            'example:\n'
            '\tInstallSymlink.py libcrypto.so.1.0.0 libcrypt.so '
            '/usr/lib/x86_64-linux-gnu False')
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
  install_dir = args.install_dir
  dst_is_dir = args.isdir
  dir_mode = int(args.mode, 8)

  DESTDIR = os.environ.get('DESTDIR')
  if DESTDIR:
      install_dir = DESTDIR + install_dir if os.path.isabs(install_dir) \
               else os.path.join(DESTDIR, install_dir)

  InstallSymlink(src, dst, install_dir, dst_is_dir, dir_mode)


if __name__ == '__main__':
  main()
