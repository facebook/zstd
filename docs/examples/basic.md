---
title: Basic Compression
description: Simple one-shot compression and decompression examples.
---

# Basic Compression

## Simplest Usage

```zig
const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const original = "Hello, zstd.zig! This text will be compressed.";

    const compressed = try zstd.compress(allocator, original, .{});
    defer allocator.free(compressed);

    const decompressed = try zstd.decompress(allocator, compressed, .{});
    defer allocator.free(decompressed);

    std.debug.print("Original: {d} bytes\n", .{original.len});
    std.debug.print("Compressed: {d} bytes\n", .{compressed.len});
    std.debug.print("Decompressed: {s}\n", .{decompressed});
}
```

## Different Compression Levels

```zig
// Fastest compression
const fast = try zstd.compress(allocator, data, .{ .level = .fastest });

// Default (balanced)
const balanced = try zstd.compress(allocator, data, .{ .level = .default });

// Best ratio
const best = try zstd.compress(allocator, data, .{ .level = .best });

// Custom level (1-22)
const custom = try zstd.compress(allocator, data, .{
    .level = @enumFromInt(12),
});
```

## With Checksum

```zig
const compressed = try zstd.compress(allocator, data, .{
    .checksum = true,
});

// Decompressor will verify checksum automatically
const decompressed = try zstd.decompress(allocator, compressed, .{});
```

## Reusable Compressor

```zig
var comp = zstd.Compressor.init(.{ .level = .default });
defer comp.deinit();

const c1 = try comp.compressAlloc(allocator, data1);
defer allocator.free(c1);

const c2 = try comp.compressAlloc(allocator, data2);
defer allocator.free(c2);
```

## Pre-allocated Buffer

```zig
var comp = zstd.Compressor.init(.{});
defer comp.deinit();

var buf: [4096]u8 = undefined;
const written = try comp.compress2(&buf, data);
std.debug.print("Compressed to {d} bytes\n", .{written});
```

## compressBound

```zig
const bound = try zstd.compressBound(src.len);
// bound >= src.len is guaranteed
var buf = try allocator.alloc(u8, bound);
defer allocator.free(buf);
```
