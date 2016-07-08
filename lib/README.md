zstd - library files
================================

The __lib__ directory contains several files, but depending on target use case, some of them may not be necessary.

#### Minimal library files

To build the zstd library the following files are required:

- [common/bitstream.h](common/bitstream.h)
- [common/error_private.h](common/error_private.h)
- [common/error_public.h](common/error_public.h)
- common/fse.h
- common/fse_decompress.c
- common/huf.h
- [common/mem.h](common/mem.h)
- [common/zstd.h]
- common/zstd_internal.h
- compress/fse_compress.c
- compress/huf_compress.c
- compress/zstd_compress.c
- compress/zstd_opt.h
- decompress/huf_decompress.c
- decompress/zstd_decompress.c

Stable API is exposed in [common/zstd.h].
Advanced and experimental API can be enabled by defining `ZSTD_STATIC_LINKING_ONLY`.
Never use them with a dynamic library, as their definition may change in future versions.

[common/zstd.h]: common/zstd.h


#### Separate compressor and decompressor

To build a separate zstd compressor all files from `common/` and `compressor/` directories are required.
In a similar way to build a separate zstd decompressor all files from `common/` and `decompressor/` directories are needed.


#### Buffered streaming

This complementary API makes streaming integration easier.
It is used by `zstd` command line utility, and [7zip plugin](http://mcmilk.de/projects/7-Zip-ZStd) :

- common/zbuff.h
- compress/zbuff_compress.c
- decompress/zbuff_decompress.c


#### Dictionary builder

In order to create dictionaries from some training sets,
it's needed to include all files from [dictBuilder directory](dictBuilder/)


#### Legacy support

Zstandard can decode previous formats, starting from v0.1.
Support for these format is provided in [folder legacy](legacy/).
It's also required to compile the library with `ZSTD_LEGACY_SUPPORT = 1`.


#### Miscellaneous

The other files are not source code. There are :

 - LICENSE : contains the BSD license text
 - Makefile : script to compile or install zstd library (static or dynamic)
 - libzstd.pc.in : for pkg-config (make install)
