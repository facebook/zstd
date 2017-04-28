# Linux Kernel Patch

There are three pieces, the `zstd_compress` and `zstd_decompress` kernel modules, the BtrFS patch, and the SquashFS patch.
The patches are based off of the linux kernel master branch, the first two on version 4.10, and the last on version 4.11.

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
* The patch seems to be working, it doesn't crash the kernel, and compresses at speeds and ratios that are expected.
  It can still use some more testing for fringe features, like printing options.

### Benchmarks

Benchmarks run on a Ubuntu 14.04 with 2 cores and 4 GiB of RAM.
The VM is running on a Macbook Pro with a 3.1 GHz Intel Core i7 processor,
16 GB of ram, and a SSD.

The compression benchmark is copying 10 copies of the
unzipped [silesia corpus](http://mattmahoney.net/dc/silesia.html) into a BtrFS
filesystem mounted with `-o compress-force={none, lzo, zlib, zstd}`.
The decompression benchmark is timing how long it takes to `tar` all 10 copies
into `/dev/null`.
The compression ratio is measured by comparing the output of `df` and `du`.
See `btrfs-benchmark.sh` for details.

| Algorithm | Compression ratio | Compression speed | Decompression speed |
|-----------|-------------------|-------------------|---------------------|
| None      | 0.99              | 504 MB/s          | 686 MB/s            |
| lzo       | 1.66              | 398 MB/s          | 442 MB/s            |
| zlib      | 2.58              | 65 MB/s           | 241 MB/s            |
| zstd 1    | 2.57              | 260 MB/s          | 383 MB/s            |
| zstd 3    | 2.71              | 174 MB/s          | 408 MB/s            |
| zstd 6    | 2.87              | 70 MB/s           | 398 MB/s            |
| zstd 9    | 2.92              | 43 MB/s           | 406 MB/s            |
| zstd 12   | 2.93              | 21 MB/s           | 408 MB/s            |
| zstd 15   | 3.01              | 11 MB/s           | 354 MB/s            |

## SquashFS

* The patch is located in `squashfs.diff`
* Additionally `fs/squashfs/zstd_wrapper.c` is provided as a source for convenience.
* The patch has been tested on a 4.6 kernel, and it builds successfully on the 4.11 source tree.

Benchmarks run on Centos 7 with 24 cores at 2.5 GHz, and 60 GiB of RAM.

The compression benchmark is the file tree from the SquashFS archive found in the
Ubuntu 16.04 desktop image (ubuntu-16.04.2-desktop-amd64.iso).
The compression benchmark uses mksquashfs with the default block size (128 KB)
and various compression algorithms/compression levels.
The decompression benchmark is timing how long it takes to `tar` the file tree
into `/dev/null`.
The compression ratio is measured by comparing the output of `df` and `du`.

Note that due to the high core count, the decompression benchmark was IO bound
rather than CPU bound unfortunately and so results for all algorithms are the same.
More accurate benchmarks should be done.

| Algorithm | Compression ratio | Compression speed | Decompression speed |
|-----------|-------------------|-------------------|---------------------|
| gzip 9    | 2.44              | 8.75 MB/s         | 1220 MB/s           |
| lzo       | 2.69              | 6.99 MB/s         | 1350 MB/s           |
| xz        | 3.15              | 4.57 MB/s         | 1280 MB/s           |
| lz4       | 1.95              | 384 MB/s          | 1300 MB/s           |
| zstd 1    | 2.49              | 215 MB/s          | 1300 MB/s           |
| zstd 5    | 2.70              | 77.1 MB/s         | 1290 MB/s           |
| zstd 10   | 2.77              | 29.6 MB/s         | 1290 MB/s           |
| zstd 15   | 2.89              | 8.56 MB/s         | 1300 MB/s           |
| zstd 22   | 2.91              | 4.59 MB/s         | 1310 MB/s           |
