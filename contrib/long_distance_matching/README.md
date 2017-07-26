This is a compression algorithm focused on finding long distance matches.

It is based upon lz4 and uses nearly the same block format (github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md). The number of bytes to encode the offset is four instead of two in lz4 to reflect the longer distance matching. The block format is descriped in `ldm.h`.

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

### Compression statistics

Compression statistics (and the configuration) can be enabled/disabled via `COMPUTE_STATS` and `OUTPUT_CONFIGURATION` in `ldm.h`.






