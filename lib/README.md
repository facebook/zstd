zstd - library files
================================

The __lib__ directory contains several files, but depending on target use case, some of them may not be necessary.

#### Minimal library files

##### Shared ressources

- [mem.h](mem.h)
- [error_private.h](error_private.h)
- [error_public.h](error_public.h)

##### zstd core compression

- [bitstream.h](bitstream.h)
- fse.c
- fse.h
- fse_static.h
- huff0.c
- huff0.h
- huff0_static.h
- zstd_compress.c
- zstd_decompress.c
- zstd_internal.h
- zstd_opt.h
- zstd.h
- zstd_static.h

#### Buffered streaming

This complementary API makes streaming integration easier.
It is used by `zstd` command line utility :

- zbuff.c
- zbuff.h
- zbuff_static.h

#### Dictionary builder

To create dictionaries from training sets :

- divsufsort.c
- divsufsort.h
- zdict.c
- zdict.h
- zdict_static.h

#### Miscellaneous

The other files are not source code. There are :

 - LICENSE : contains the BSD license text
 - Makefile : script to compile or install zstd library (static or dynamic)
 - libzstd.pc.in : for pkg-config (make install)

