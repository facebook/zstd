# Linux Kernel Patch

There are two pieces, the `zstd_compress` and `zstd_decompress` kernel modules, and the BtrFS patch.
The patches are based off of the linux kernel master branch (version 4.10).

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

* The patch is located in `btrfs.diff`.
* Additionally `fs/btrfs/zstd.c` is provided as a source for convenience.
* The patch seems to be working, it doesn't crash the kernel, and compresses at speeds and ratios athat are expected.
  It can still use some more testing for fringe features, like printing options.
