---
title: Streaming
description: Streaming compression and decompression examples.
---

# Streaming Examples

## Streaming Compression

```zig
const std = @import("std");
const zstd = @import("zstd");

pub fn compressFile(allocator: std.mem.Allocator, input: []const u8) ![]u8 {
    var comp = zstd.StreamCompressor.init(allocator, .{
        .level = .default,
    });
    defer comp.deinit();

    var output: [zstd.recommendedCOutSize()]u8 = undefined;
    var result = std.ArrayList(u8).empty;
    defer result.deinit(allocator);

    // Process input in chunks
    var pos: usize = 0;
    while (pos < input.len) {
        const end = @min(pos + zstd.recommendedCInSize(), input.len);
        const chunk = input[pos..end];

        const r = try comp.compressChunk(chunk, &output, .@"continue");
        if (r.bytes_written > 0) {
            try result.appendSlice(allocator, output[0..r.bytes_written]);
        }
        pos = end;
    }

    // Finalize
    const final = try comp.endStream(&output);
    if (final.bytes_written > 0) {
        try result.appendSlice(allocator, output[0..final.bytes_written]);
    }

    return result.toOwnedSlice(allocator);
}
```

## Streaming Decompression

```zig
pub fn decompressStream(allocator: std.mem.Allocator, compressed: []const u8) ![]u8 {
    var decomp = zstd.StreamDecompressor.init(allocator, .{});
    defer decomp.deinit();

    var output: [zstd.recommendedDOutSize()]u8 = undefined;
    var result = std.ArrayList(u8).empty;
    defer result.deinit(allocator);

    var pos: usize = 0;
    while (pos < compressed.len) {
        const end = @min(pos + zstd.recommendedDInSize(), compressed.len);
        const chunk = compressed[pos..end];

        const r = try decomp.decompressChunk(chunk, &output);
        if (r.bytes_written > 0) {
            try result.appendSlice(allocator, output[0..r.bytes_written]);
        }
        pos = end;
    }

    return result.toOwnedSlice(allocator);
}
```

## Multi-Chunk Round Trip

```zig
// Compress in chunks
var comp = zstd.StreamCompressor.init(allocator, .{});
defer comp.deinit();

var c_buf: [zstd.recommendedCOutSize()]u8 = undefined;

const r1 = try comp.compressChunk("Hello, ", &c_buf, .@"continue");
const r2 = try comp.compressChunk("World!", &c_buf, .@"continue");
const r3 = try comp.endStream(&c_buf);

// Collect compressed data...
// Then decompress
var decomp = zstd.StreamDecompressor.init(allocator, .{});
defer decomp.deinit();

var d_buf: [zstd.recommendedDOutSize()]u8 = undefined;
const d1 = try decomp.decompressChunk(compressed_chunk, &d_buf);
std.debug.print("{s}\n", .{d_buf[0..d1.bytes_written]}); // "Hello, World!"
```
