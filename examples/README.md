Zstandard library : usage examples
==================================

- [Simple compression](simple_compression.c)
  Compress a single file.
  Introduces usage of : `ZSTD_compress()`

- [Simple decompression](simple_decompression.c)
  Decompress a single file compressed by zstd.
  Introduces usage of : `ZSTD_decompress()`

- [Dictionary compression](dictionary_compression.c)
  Compress multiple files using the same dictionary.
  Introduces usage of : `ZSTD_createCDict()` and `ZSTD_compress_usingCDict()`

- [Dictionary decompression](dictionary_decompression.c)
  Decompress multiple files using the same dictionary.
  Introduces usage of : `ZSTD_createDDict()` and `ZSTD_decompress_usingDDict()`
