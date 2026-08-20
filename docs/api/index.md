---
title: API Reference
description: Complete API reference for zstd.zig compression library.
---

# API Reference

## Top-Level Functions

### `compress`

One-shot compression.

```zig
pub fn compress(allocator: std.mem.Allocator, src: []const u8, opts: CompressOptions) ZstdError![]u8
```

### `decompress`

One-shot decompression.

```zig
pub fn decompress(allocator: std.mem.Allocator, src: []const u8, opts: DecompressOptions) ZstdError![]u8
```

### `compressBound`

Maximum compressed size for a given input.

```zig
pub fn compressBound(src_size: usize) ZstdError!usize
```

## Types

| Type | Description |
|------|-------------|
| [CompressOptions](/api/compress-options) | Options for compression |
| [DecompressOptions](/api/decompress-options) | Options for decompression |
| [Compressor](/api/compressor) | Reusable compression context |
| [Decompressor](/api/decompressor) | Reusable decompression context |
| [StreamCompressor](/api/stream-compressor) | Streaming compression |
| [StreamDecompressor](/api/stream-decompressor) | Streaming decompression |
| [CDict / DDict](/api/dict) | Dictionary compression types |
| [Frame](/api/frame) | Frame inspection functions |
| [CLevel](/api/clevel) | Compression level enum |
| [Constants](/api/constants) | Library constants |
| [Errors](/api/errors) | Error types |

## Namespaces

```zig
zstd.Frame      // Frame inspection
zstd.version    // Version information
zstd.constants  // Library constants
```

## Backward-Compatible Aliases

These names are still available but prefer the new API:

```zig
zstd.versionNumber()    // → zstd.version.number
zstd.versionString()    // → zstd.version.string
zstd.minCLevel()        // → zstd.version.clevel_min
zstd.maxCLevel()        // → zstd.version.clevel_max
zstd.defaultCLevel()    // → zstd.version.clevel_default
zstd.isFrame()          // → zstd.Frame.isFrame()
zstd.getFrameContentSize() // → zstd.Frame.contentSize()
zstd.findFrameCompressedSize() // → zstd.Frame.compressedSize()
```
