Educational Decoder
===================

`zstd_decompress.c` is a self-contained implementation in C99 of a decoder,
according to the [Zstandard format specification].
While it does not implement as many features as the reference decoder,
such as the streaming API or content checksums, it is written to be easy to
follow and understand, to help understand how the Zstandard format works.
It's laid out to match the [format specification],
so it can be used to understand how complex segments could be implemented.
It also contains implementations of Huffman and FSE table decoding.

[Zstandard format specification]: https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md
[format specification]: https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md

`harness.c` provides a simple test harness around the decoder:

    harness <input-file> <output-file> [dictionary]

