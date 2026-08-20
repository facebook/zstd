---
title: StreamDecompressor
description: Chunk-based streaming decompression for large data.
---

# StreamDecompressor

Process compressed data in chunks for streaming decompression.

## Definition

```zig
pub const StreamDecompressor = struct {
    allocator: std.mem.Allocator,
    // ...
};
```

## Methods

### `init`

```zig
pub fn init(allocator: std.mem.Allocator, opts: StreamDecompressOptions) StreamDecompressor
```

```zig
var decomp = zstd.StreamDecompressor.init(allocator, .{});
defer decomp.deinit();
```

### `deinit`

```zig
pub fn deinit(self: *StreamDecompressor) void
```

### `decompressChunk`

Decompress a chunk of data.

```zig
pub fn decompressChunk(self: *StreamDecompressor, input: []const u8, output: []u8) ZstdError!StreamResult
```

```zig
const result = try decomp.decompressChunk(compressed_chunk, &output);
try process(output[0..result.bytes_written]);
```

## StreamDecompressOptions

```zig
pub const StreamDecompressOptions = struct {
    dict: ?[]const u8 = null,
};
```

## Recommended Sizes

```zig
const in_size = zstd.recommendedDInSize();   // 128 KB
const out_size = zstd.recommendedDOutSize();  // 128 KB
```
