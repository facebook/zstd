 **Zstd**, short for Zstandard, is a new lossless compression algorithm, which provides both good compression ratio _and_ speed for your standard compression needs. "Standard" translates into everyday situations which neither look for highest possible ratio (which LZMA and ZPAQ cover) nor extreme speeds (which LZ4 covers).

It is provided as a BSD-license package, hosted on Github.

|Branch      |Status   |
|------------|---------|
|master      | [![Build Status](https://travis-ci.org/Cyan4973/zstd.svg?branch=master)](https://travis-ci.org/Cyan4973/zstd) |
|dev         | [![Build Status](https://travis-ci.org/Cyan4973/zstd.svg?branch=dev)](https://travis-ci.org/Cyan4973/zstd) |

For a taste of its performance, here are a few benchmark numbers, completed on a Core i5-4300U @ 1.9 GHz, using [fsbench 0.14.3](http://encode.ru/threads/1371-Filesystem-benchmark?p=34029&viewfull=1#post34029), an open-source benchmark program by m^2.

|Name           | Ratio | C.speed | D.speed |
|---------------|-------|---------|---------|
|               |       |   MB/s  |  MB/s   |
| [zlib 1.2.8 -6](http://www.zlib.net/)| 3.099 |    18   |  275    |
| **zstd**      |**2.872**|**201**|**498**  |
| [zlib 1.2.8 -1](http://www.zlib.net/)| 2.730 |    58   |   250   |
| [LZ4 HC r127](https://github.com/Cyan4973/lz4)| 2.720 |   26    |  1720   |
| QuickLZ 1.5.1b6|2.237 |  323    |  373    |
| LZO 2.06      | 2.106 |  351    |  510    |
| Snappy 1.1.0  | 2.091 |  238    |  964    |
| [LZ4 r127](https://github.com/Cyan4973/lz4)| 2.084 |  370    | 1590    |
| LZF 3.6       | 2.077 |  220    |  502    |

An interesting feature of zstd is that it can qualify as both a reasonably strong compressor and a fast one.

Zstd delivers high decompression speed, at around ~500 MB/s per core.
Obviously, your exact mileage will vary depending on your target system.

Zstd compression speed, on the other hand, can be configured to fit different situations.
The first, fast, derivative offers ~200 MB/s per core, which is suitable for a few real-time scenarios.
But similar to [LZ4](https://github.com/Cyan4973/lz4), zstd can offer derivatives trading compression time for compression ratio, while keeping decompression properties intact. "Offline compression", where compression time is of little importance because the content is only compressed once and decompressed many times, is therefore within the scope.

Note that high compression derivatives still have to be developed.
It's a complex area which will certainly benefit the contributions from a few experts.


Another property zstd is developed for is configurable memory requirement, with the objective to fit into low-memory configurations, or servers handling many connections in parallel.

Zstd entropy stage is provided by [FSE (Finite State Entropy)](https://github.com/Cyan4973/FiniteStateEntropy).

Zstd development is starting. So consider current results merely as early ones. The implementation will gradually evolve and improve overtime, especially during this first year. This is a phase which will depend a lot on user feedback, since these feedback will be key in deciding next priorities or features to add.

The "master" branch is reserved for stable release and betas.
The "dev" branch is the one where all contributions will be merged. If you plan to propose a patch, please commit into the "dev" branch. Direct commit to "master" are not permitted.
Feature branches will also exist, typically to introduce new requirements, and be temporarily available for testing before merge into "dev" branch.
