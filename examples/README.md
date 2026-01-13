# Zstandard Zig Examples

This directory contains Zig examples demonstrating the usage of the zstd library.

- **[Simple compression](simple_compression.zig)**:
  Compress a single file.
  Usage: `zig build run-simple_compression -- <file>`

- **[Simple decompression](simple_decompression.zig)**:
  Decompress a single file (in memory).
  Usage: `zig build run-simple_decompression -- <file.zst>`

- **[Multiple simple compression](multiple_simple_compression.zig)**:
  Compress multiple files reusing resources.
  Usage: `zig build run-multiple_simple_compression -- <file1> <file2> ...`

- **[Streaming memory usage](streaming_memory_usage.zig)**:
  Check memory usage of streaming context.
  Usage: `zig build run-streaming_memory_usage`

- **[Streaming compression](streaming_compression.zig)**:
  Compress a single file using streaming API.
  Usage: `zig build run-streaming_compression -- <file>`

- **[Multiple Streaming compression](multiple_streaming_compression.zig)**:
  Compress multiple files reusing streaming resources.
  Usage: `zig build run-multiple_streaming_compression -- <file1> <file2> ...`

- **[Streaming decompression](streaming_decompression.zig)**:
  Decompress a file to stdout using streaming API.
  Usage: `zig build run-streaming_decompression -- <file.zst>`

- **[Dictionary compression](dictionary_compression.zig)**:
  Compress multiple files using a dictionary.
  Usage: `zig build run-dictionary_compression -- <file1> <file2> <dictionary>`

- **[Dictionary decompression](dictionary_decompression.zig)**:
  Decompress multiple files using a dictionary.
  Usage: `zig build run-dictionary_decompression -- <file1.zst> <file2.zst> <dictionary>`
