This is a compression algorithm focused on finding long distance matches.

It is based upon lz4 and uses nearly the same block format (github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md). The number of bytes to encode the offset is four instead of two in lz4 to reflect the longer distance matching. The block format is described in `ldm.h`.

### Build

Run `make`.

### Compressing a file

`ldm <filename>`

Decompression and verification can be enabled by defining `DECOMPRESS_AND_VERIFY` in `main.c`.
The output file names are as follows:
- `<filename>.ldm` : compressed file
- `<filename>.ldm.dec` : decompressed file

### Parameters

There are various parameters that can be tuned. These parameters can be tuned in `ldm.h` or, alternatively if `ldm_params.h` is included, in `ldm_params.h` (for easier configuration).

The parameters are as follows and must all be defined:
- `LDM_MEMORY_USAGE` : the memory usage of the underlying hash table in bytes.
- `HASH_BUCKET_SIZE_LOG` : the log size of each bucket in the hash table (used in collision resolution).
- `LDM_LAG` : the lag (in bytes) in inserting entries into the hash table.
- `LDM_WINDOW_SIZE_LOG` : the log maximum window size when searching for matches.
- `LDM_MIN_MATCH_LENGTH` : the minimum match length.
- `INSERT_BY_TAG` : insert entries into the hash table as a function of the hash. This increases speed by reducing the number of hash table lookups and match comparisons. Certain hashes will never be inserted.
- `USE_CHECKSUM`  : store a checksum with the hash table entries for faster comparison. This halves the number of entries the hash table can contain.

The optional parameter `HASH_ONLY_EVERY_LOG` is the log inverse frequency of insertion into the hash table. That is, an entry is inserted approximately every `1 << HASH_ONLY_EVERY_LOG` times. If this parameter is not defined, the value is computed as a function of the window size and memory usage to approximate an even coverage of the window.


### Benchmark

Below is a comparison of various compression methods on a tar of four versions of llvm (versions `3.9.0`, `3.9.1`, `4.0.0`, `4.0.1`) with a total size of `727900160` B.

| Method | Size | Ratio |
|:---|---:|---:|
|lrzip -p 32 -n -w 1 | `369968714` | `1.97`|
|ldm | `209391361` | `3.48`|
|lz4 | `189954338` | `3.83`|
|lrzip -p 32 -l -w 1 | `163940343` | `4.44`|
|zstd -1 | `126080293` | `5.77`|
|lrzip -p 32 -n | `124821009` | `5.83`|
|lrzip -p 32 -n -w 1 & zstd -1 | `120317909` | `6.05`|
|zstd -3 -o | `115290952` | `6.31`|
|lrzip -p 32 -g -L 9 -w 1 | `107168979` | `6.79`|
|zstd -6 -o | `102772098` | `7.08`|
|zstd -T16 -9 | `98040470` | `7.42`|
|lrzip -p 32 -n -w 1 & zstd -T32 -19 | `88050289` | `8.27`|
|zstd -T32 -19 | `83626098` | `8.70`|
|lrzip -p 32 -n & zstd -1 | `36335117` | `20.03`|
|ldm & zstd -6 | `32856232` | `22.15`|
|lrzip -p 32 -g -L 9 | `32243594` | `22.58`|
|lrzip -p 32 -n & zstd -6 | `30954572` | `23.52`|
|lrzip -p 32 -n & zstd -T32 -19 | `26472064` | `27.50`|

The method marked `ldm` was run with the following parameters:

| Parameter | Value |
|:---|---:|
| `LDM_MEMORY_USAGE`    |   `23`|
|`HASH_BUCKET_SIZE_LOG` |    `3`|
|`LDM_LAG`              |    `0`|
|`LDM_WINDOW_SIZE_LOG`  |   `28`|
|`LDM_MIN_MATCH_LENGTH`|   `64`|
|`INSERT_BY_TAG`        |    `1`|
|`USE_CHECKSUM`         |    `1`|

The compression speed was `220.5 MB/s`.

### Parameter selection

Below is a brief discussion of the effects of the parameters on the speed and compression ratio.

#### Speed

A large bottleneck in terms of speed is finding the matches and comparing to see if they are greater than the minimum match length. Generally:
- The fewer matches found (or the lower the percentage of the literals matched), the slower the algorithm will behave.
- Increasing `HASH_ONLY_EVERY_LOG` results in fewer inserts and, if `INSERT_BY_TAG` is set, fewer lookups in the table. This has a large effect on speed, as well as compression ratio.
- If `HASH_ONLY_EVERY_LOG` is not set, its value is calculated based on `LDM_WINDOW_SIZE_LOG` and `LDM_MEMORY_USAGE`. Increasing `LDM_WINDOW_SIZE_LOG` has the effect of increasing `HASH_ONLY_EVERY_LOG` and increasing `LDM_MEMORY_USAGE` decreases `HASH_ONLY_EVERY_LOG`.
- `USE_CHECKSUM` generally improves speed with hash table lookups.

#### Compression ratio

The compression ratio is highly correlated with the coverage of matches. As a long distance matcher, the algorithm was designed to "optimize" for long distance matches outside the zstd compression window. The compression ratio after recompressing the output of the long-distance matcher with zstd was a more important signal in development than the raw compression ratio itself.

Generally, increasing `LDM_MEMORY_USAGE` will improve the compression ratio. However when using the default computed value of `HASH_ONLY_EVERY_LOG`, this increases the frequency of insertion and lookup in the table and thus may result in a decrease in speed. 

Below is a table showing the speed and compression ratio when compressing the llvm tar (as described above) using different settings for `LDM_MEMORY_USAGE`. The other parameters were the same as used in the benchmark above.

| `LDM_MEMORY_USAGE` | Ratio | Speed (MB/s) | Ratio after zstd -6  |
|---:| ---: | ---: | ---: |
| `18` | `1.85` | `232.4` | `10.92` |
| `21` | `2.79` | `233.9` | `15.92` |
| `23` | `3.48` | `220.5` | `18.29` |
| `25` | `4.56` | `140.8` | `19.21` |

### Compression statistics

Compression statistics (and the configuration) can be enabled/disabled via `COMPUTE_STATS` and `OUTPUT_CONFIGURATION` in `ldm.h`.
