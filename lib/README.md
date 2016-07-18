zstd - library files
================================

The __lib__ directory contains several directories.
Depending on target use case, it's enough to include only files from relevant directories.


#### API

Zstandard's stable API is exposed within [zstd.h](zstd.h),
at the root of `lib` directory.


#### Advanced API

Some additional API may be useful if you're looking into advanced features :
- common/error_public.h : transforms `size_t` function results into an `enum`,
                          for precise error handling.
- ZSTD_STATIC_LINKING_ONLY : if you define this macro _before_ including `zstd.h`,
                          it will give access to advanced and experimental API.
                          These APIs shall ___never be used with dynamic library___ !
                          They are not "stable", their definition may change in the future.
                          Only static linking is allowed.


#### Modular build

Directory `common/` is required in all circumstances.
You can select to support compression only, by just adding files from the `compress/` directory,
In a similar way, you can build a decompressor-only library with the `decompress/` directory.

Other optional functionalities provided are :

- `dictBuilder/`  : source files to create dictionaries.
                    The API can be consulted in `dictBuilder/zdict.h`.
                    This module also depends on `common/` and `compress/` .

- `legacy/` : source code to decompress previous versions of zstd, starting from `v0.1`.
              This module also depends on `common/` and `decompress/` .
              Note that it's required to compile the library with `ZSTD_LEGACY_SUPPORT = 1` .
              The main API can be consulted in `legacy/zstd_legacy.h`.
              Advanced API from each version can be found in its relevant header file.
              For example, advanced API for version `v0.4` is in `zstd_v04.h` .


#### Streaming API

Streaming is currently provided by `common/zbuff.h`.


#### Miscellaneous

The other files are not source code. There are :

 - LICENSE : contains the BSD license text
 - Makefile : script to compile or install zstd library (static and dynamic)
 - libzstd.pc.in : for pkg-config (`make install`)
