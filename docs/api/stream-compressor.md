---
title: StreamCompressor
description: Chunk-based streaming compression for large data.
---

# StreamCompressor

Process data in chunks for streaming compression. Useful when data doesn't fit in memory.

## Definition

```zig
pub const StreamCompressor = struct {
    compression_level: i32,
    checksum: bool,
    frame_started: bool,
    buffer: std.ArrayList(u8),
    allocator: std.mem.Allocator,
    // ...
};
```

## Methods

### `init`

```zig
pub fn init(allocator: std.mem.Allocator, opts: StreamCompressOptions) StreamCompressor
```

```zig
var comp = zstd.StreamCompressor.init(allocator, .{
    .level = .default,
    .checksum = false,
});
defer comp.deinit();
```

### `deinit`

```zig
pub fn deinit(self: *StreamCompressor) void
```

### `compressChunk`

Feed data into the stream.

```zig
pub fn compressChunk(self: *StreamCompressor, input: []const u8, output: []u8, end_op: EndDirective) ZstdError!StreamResult
```

```zig
const result = try comp.compressChunk(data, &output, .@"continue");
if (result.bytes_written > 0) {
    try send(output[0..result.bytes_written]);
}
```

### `endStream`

Finalize the stream.

```zig
pub fn endStream(self: *StreamCompressor, output: []u8) ZstdError!StreamResult
```

### `flushStream`

Flush buffered data.

```zig
pub fn flushStream(self: *StreamCompressor, output: []u8) ZstdError!StreamResult
```

### `reset`

Reset for reuse.

```zig
pub fn reset(self: *StreamCompressor) void
```

### `setParameter`

```zig
pub fn setParameter(self: *StreamCompressor, param: CParameter, value: i32) void
```

## StreamCompressOptions

```zig
pub const StreamCompressOptions = struct {
    level: CLevel = .default,
    checksum: bool = false,
};
```

## Recommended Sizes

```zig
const in_size = zstd.recommendedCInSize();   // 128 KB
const out_size = zstd.recommendedCOutSize();  // 128 KB + 6
```
