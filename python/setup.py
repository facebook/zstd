#!/usr/bin/env python

from setuptools import setup, find_packages, Extension

VERSION = (0, 0, 1)

setup(
    name='zstd',
    version=".".join([str(x) for x in VERSION]),
    description="Zstd Bindings for Python",
    author='Sergey Dryabzhinsky',
    author_email='sergey.dryabzhinsky@gmail.com',
    url='https://github.com/Cyan4973/zstd',
    packages=find_packages('src'),
    package_dir={'': 'src'},
    ext_modules=[
        Extension('zstd',
            sources=[
                'src/lib/zstd.c',
                'src/python-zstd.c'
            ],
            depends=[
                'src/lib/zstd.h',
                'src/python-zstd.c'
            ],
            include_dirs = ['src/lib'],
            extra_compile_args=[
                "-std=c99",
                "-O2",
                "-Wall",
                "-W",
                "-Wundef",
#            "-DFORTIFY_SOURCE=2", "-fstack-protector",
#            "-march=native",
#            "-floop-interchange", "-floop-block", "-floop-strip-mine", "-ftree-loop-distribution",
        ])
    ],
    classifiers=[
        'License :: OSI Approved :: BSD License',
        'Intended Audience :: Developers',
        'Programming Language :: C',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.2',
    ],
)
