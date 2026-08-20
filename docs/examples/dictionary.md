---
title: Dictionary Compression
description: Dictionary-based compression examples for better ratios on similar data.
---

# Dictionary Compression

Dictionary compression achieves much better ratios when compressing many similar small buffers.

## Basic Dictionary Usage

```zig
const std = @import("std");
const zstd = @import("zstd");

// Create dictionaries from raw bytes
var cdict = zstd.CDict.init(dict_data, 3);
defer cdict.deinit();

var ddict = zstd.DDict.init(dict_data);
defer ddict.deinit();

// Compress with CDict
const compressed = try cdict.compress(allocator, data);
defer allocator.free(compressed);

// Decompress with DDict
const decompressed = try ddict.decompress(allocator, compressed);
defer allocator.free(decompressed);
```

## Training a Dictionary

```zig
// Prepare sample data
const sample1 = "The quick brown fox jumps over the lazy dog";
const sample2 = "A quick brown fox leaps over a lazy dog";
const sample3 = "The fast brown fox jumps above the lazy dog";

var samples_buf: [sample1.len + sample2.len + sample3.len]u8 = undefined;
var offset: usize = 0;
for ([_][]const u8{ sample1, sample2, sample3 }) |s| {
    @memcpy(samples_buf[offset..][0..s.len], s);
    offset += s.len;
}

const sizes = &[_]usize{ sample1.len, sample2.len, sample3.len };

// Train (requires at least 3 samples)
const dict = try zstd.trainFromSamples(&samples_buf, sizes, 112640);
defer allocator.free(dict);
```

## Dictionary from Frame

```zig
// Check if a compressed frame uses a dictionary
const dict_id = zstd.Frame.dictId(compressed);
if (dict_id != 0) {
    std.debug.print("Frame uses dictionary ID: {d}\n", .{dict_id});
}
```

## Using compressUsingDict

```zig
// With raw dictionary bytes
const compressed = try zstd.compressUsingDict(
    allocator,
    data,
    dict_bytes,
    3, // compression level
);
defer allocator.free(compressed);

// Decompress with same dictionary
const decompressed = try zstd.decompressUsingDict(
    allocator,
    compressed,
    dict_bytes,
);
defer allocator.free(decompressed);
```
