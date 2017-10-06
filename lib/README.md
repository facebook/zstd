Zstandard library files
================================

The __lib__ directory is split into several sub-directories,
in order to make it easier to select or exclude specific features.


#### Building

`Makefile` script is provided, supporting the standard set of commands,
directories, and variables (see https://www.gnu.org/prep/standards/html_node/Command-Variables.html).
- `make` : generates both static and dynamic libraries
- `make install` : install libraries in default system directories


#### API

Zstandard's stable API is exposed within [lib/zstd.h](zstd.h).


#### Advanced API

Optional advanced features are exposed via :

- `lib/common/zstd_errors.h` : translates `size_t` function results
                              into an `ZSTD_ErrorCode`, for accurate error handling.
- `ZSTD_STATIC_LINKING_ONLY` : if this macro is defined _before_ including `zstd.h`,
                          it unlocks access to advanced experimental API,
                          exposed in second part of `zstd.h`.
                          These APIs shall ___never be used with dynamic library___ !
                          They are not "stable", their definition may change in the future.
                          Only static linking is allowed.


#### Modular build

- Directory `lib/common` is always required, for all variants.
- Compression source code lies in `lib/compress`
- Decompression source code lies in `lib/decompress`
- It's possible to include only `compress` or only `decompress`, they don't depend on each other.
- `lib/dictBuilder` : makes it possible to generate dictionaries from a set of samples.
    The API is exposed in `lib/dictBuilder/zdict.h`.
    This module depends on both `lib/common` and `lib/compress` .
- `lib/legacy` : source code to decompress older zstd formats, starting from `v0.1`.
              This module depends on `lib/common` and `lib/decompress`.
              To enable this feature, it's necessary to define `ZSTD_LEGACY_SUPPORT = 1` during compilation.
              Typically, with `gcc`, add argument `-DZSTD_LEGACY_SUPPORT=1`.
              Using higher number limits the number of version supported.
              For example, `ZSTD_LEGACY_SUPPORT=2` means : "support legacy formats starting from v0.2+".
              The API is exposed in `lib/legacy/zstd_legacy.h`.
              Each version also provides a (dedicated) set of advanced API.
              For example, advanced API for version `v0.4` is exposed in `lib/legacy/zstd_v04.h` .


#### Multithreading support

Multithreading is disabled by default when building with `make`.
Enabling multithreading requires 2 conditions :
- set macro `ZSTD_MULTITHREAD`
- on POSIX systems : compile with pthread (`-pthread` compilation flag for `gcc` for example)

Both conditions are automatically triggered by invoking `make lib-mt` target.
Note that, when linking a POSIX program with a multithreaded version of `libzstd`,
it's necessary to trigger `-pthread` flag during link stage.

Multithreading capabilities are exposed via :
- private API `lib/compress/zstdmt_compress.h`.
  Symbols defined in this header are currently exposed in `libzstd`, hence usable.
  Note however that this API is planned to be locked and remain strictly internal in the future.
- advanced API `ZSTD_compress_generic()`, defined in `lib/zstd.h`, experimental section.
  This API is still considered experimental, but is designed to be labelled "stable" at some point in the future.
  It's the recommended entry point for multi-threading operations.


#### Windows : using MinGW+MSYS to create DLL

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


#### Deprecated API

Obsolete API on their way out are stored in directory `lib/deprecated`.
At this stage, it contains older streaming prototypes, in `lib/deprecated/zbuff.h`.
Presence in this directory is temporary.
These prototypes will be removed in some future version.
Consider migrating code towards supported streaming API exposed in `zstd.h`.


#### Miscellaneous

The other files are not source code. There are :

 - `LICENSE` : contains the BSD license text
 - `Makefile` : `make` script to build and install zstd library (static and dynamic)
 - `BUCK` : support for `buck` build system (https://buckbuild.com/)
 - `libzstd.pc.in` : for `pkg-config` (used in `make install`)
 - `README.md` : this file
