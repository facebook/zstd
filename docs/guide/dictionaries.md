---
title: Dictionaries
description: Use dictionary compression for better ratios on similar data.
---

# Dictionaries

Dictionary compression achieves significantly better compression ratios when compressing many similar small buffers (e.g., database records, JSON objects, log entries).

## CDict (Compression Dictionary)

```zig
const zstd = @import("zstd");

// Create a CDict from dictionary data
var cdict = zstd.CDict.init(dict_data, 3);
defer cdict.deinit();

// Compress using the dictionary
const compressed = try cdict.compress(allocator, data);
defer allocator.free(compressed);
```

## DDict (Decompression Dictionary)

```zig
var ddict = zstd.DDict.init(dict_data);
defer ddict.deinit();

// Decompress using the dictionary
const decompressed = try ddict.decompress(allocator, compressed);
defer allocator.free(decompressed);
```

## Dictionary Functions

### Compress with raw dictionary bytes

```zig
const compressed = try zstd.compressUsingDict(allocator, data, dict_bytes, 3);
defer allocator.free(compressed);
```

### Decompress with raw dictionary bytes

```zig
const decompressed = try zstd.decompressUsingDict(allocator, compressed, dict_bytes);
defer allocator.free(decompressed);
```

### Get Dictionary ID

```zig
// From dictionary data
const dict_id = zstd.getDictIDFromDict(dict_bytes);

// From compressed frame
const frame_dict_id = zstd.getDictIDFromFrame(compressed);
```

## Training a Dictionary

Train a dictionary from sample data:

```zig
const samples = &[_][]const u8{
    sample1, sample2, sample3, sample4, sample5,
};

// Get total buffer size
var total: usize = 0;
for (samples) |s| total += s.len;

// Create sizes array
var sizes: [samples.len]usize = undefined;
for (samples, 0..) |s, i| sizes[i] = s.len;

// Create samples buffer
var buf = try allocator.alloc(u8, total);
defer allocator.free(buf);
var offset: usize = 0;
for (samples) |s| {
    @memcpy(buf[offset..][0..s.len], s);
    offset += s.len;
}

// Train the dictionary (requires at least 3 samples)
const dict = try zstd.trainFromSamples(buf, &sizes, 112640);
defer allocator.free(dict);
```

## Finalize a Dictionary

```zig
const written = try zstd.finalizeDictionary(
    &dict_buffer,
    max_dict_size,
    dict_content,
    samples_buffer,
    &sample_sizes,
    .{ .dict_id = 0 },
);
```
