Zstandard library : usage examples
==================================

- [Dictionary decompression](dictionary_decompression.c)
  Decompress multiple files using the same dictionary.
  Compatible with Legacy modes.
  Introduces usage of : `ZSTD_createDDict()` and `ZSTD_decompress_usingDDict()`
