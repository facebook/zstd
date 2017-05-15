# Copyright (c) 2016-present, Dima Krasner
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.

import os
from distutils.core import setup, Extension

setup(name='zstd',
      version='1.0.0',
      license='BSD',
      url='https://github.com/facebook/zstd',
      author='Dima Krasner',
      author_email='dima@dimakrasner.com',
      description='Python bindings for the Zstandard compression library',
      ext_modules=[Extension('zstd',
                   sources=sum([[os.path.join(x, y) for y in os.listdir(x) if y.endswith('.c')] for x in (os.path.join(os.pardir, 'lib', z) for z in ('common', 'compress', 'decompress'))], ['pyzstd.c']),
                   include_dirs=(os.path.join(os.pardir, 'lib'), os.path.join(os.pardir, 'lib', 'common')),
                   language='c')])
