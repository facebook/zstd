Zstandard library files
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

#### ZSTDMT API

To enable multithreaded compression within the library, invoke `make lib-mt` target.
Prototypes are defined in header file `compress/zstdmt_compress.h`.
When linking a program that uses ZSTDMT API against libzstd.a on a POSIX system,
`-pthread` flag must be provided to the compiler and linker.
Note : ZSTDMT prototypes can still be used with a library built without multithread support,
but in this case, they will be single threaded only.

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
              Library compilation must include directive `ZSTD_LEGACY_SUPPORT = 1` .
              The main API can be consulted in `legacy/zstd_legacy.h`.
              Advanced API from each version can be found in their relevant header file.
              For example, advanced API for version `v0.4` is in `legacy/zstd_v04.h` .


#### Using MinGW+MSYS to create DLL

DLL can be created using MinGW+MSYS with the `make libzstd` command.
This command creates `dll\libzstd.dll` and the import library `dll\libzstd.lib`.
The import library is only required with Visual C++.
The header file `zstd.h` and the dynamic library `dll\libzstd.dll` are required to
compile a project using gcc/MinGW.
The dynamic library has to be added to linking options.
It means that if a project that uses ZSTD consists of a single `test-dll.c`
file it should be linked with `dll\libzstd.dll`. For example:
```
    gcc $(CFLAGS) -Iinclude/ test-dll.c -o test-dll dll\libzstd.dll
```
The compiled executable will require ZSTD DLL which is available at `dll\libzstd.dll`.


#### Obsolete streaming API

Streaming is now provided within `zstd.h`.
Older streaming API is still available within `deprecated/zbuff.h`.
It will be removed in a future version.
Consider migrating code towards newer streaming API in `zstd.h`.


#### Miscellaneous

The other files are not source code. There are :

 - LICENSE : contains the BSD license text
 - Makefile : script to compile or install zstd library (static and dynamic)
 - libzstd.pc.in : for pkg-config (`make install`)
 - README.md : this file
