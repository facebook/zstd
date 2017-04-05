# Linux Kernel Patch

There are two pieces, the `zstd_compress` and `zstd_decompress` kernel modules, and the BtrFS patch.
The patches are based off of the linux kernel version 4.9.
The BtrFS patch is not present in its entirety yet.

## Zstd Kernel modules

* The header is in `include/linux/zstd.h`.
* It is split up into `zstd_compress` and `zstd_decompress`, which can be loaded independently.
* Source files are in `lib/zstd/`.
* `lib/Kconfig` and `lib/Makefile` need to be modified by applying `lib/Kconfig.diff` and `lib/Makefile.diff` respectively.
* `test/UserlandTest.cpp` contains tests for the patch in userland by mocking the kernel headers.
  It can be run with the following commands:
  ```
  cd test
  make googletest
  make UserlandTest
  ./UserlandTest
  ```

## BtrFS

* `fs/btrfs/zstd.c` is provided.
* Some more glue is required to integrate it with BtrFS, but I haven't included the patches yet.
  In the meantime see https://github.com/terrelln/linux/commit/1914f7d4ca6c539369c84853eafa4ac104883047 if you're interested.
