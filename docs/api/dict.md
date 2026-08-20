---
title: CDict / DDict
description: Dictionary compression and decompression types.
---

# CDict / DDict

## CDict (Compression Dictionary)

Prepared dictionary for compression.

### Definition

```zig
pub const CDict = struct {
    dict_data: []const u8,
    compression_level: i32,
    dict_id: u32,
    // ...
};
```

### Methods

| Method | Description |
|--------|-------------|
| `init(dict_buffer, level)` | Create CDict from dictionary bytes |
| `deinit()` | Release resources |
| `compress(allocator, src)` | Compress using this dictionary |
| `getDictId()` | Get the dictionary ID |
| `sizeof()` | Get memory usage |

### Usage

```zig
var cdict = zstd.CDict.init(dict_data, 3);
defer cdict.deinit();

const compressed = try cdict.compress(allocator, data);
defer allocator.free(compressed);
```

## DDict (Decompression Dictionary)

Prepared dictionary for decompression.

### Definition

```zig
pub const DDict = struct {
    dict_data: []const u8,
    dict_id: u32,
    // ...
};
```

### Methods

| Method | Description |
|--------|-------------|
| `init(dict_buffer)` | Create DDict from dictionary bytes |
| `deinit()` | Release resources |
| `decompress(allocator, src)` | Decompress using this dictionary |
| `getDictId()` | Get the dictionary ID |
| `sizeof()` | Get memory usage |

### Usage

```zig
var ddict = zstd.DDict.init(dict_data);
defer ddict.deinit();

const decompressed = try ddict.decompress(allocator, compressed);
defer allocator.free(decompressed);
```

## DictParams

```zig
pub const DictParams = struct {
    compression_level: i32 = 0,
    notification_level: u32 = 0,
    dict_id: u32 = 0,
};
```

## Dictionary Functions

| Function | Description |
|----------|-------------|
| `compressUsingDict(alloc, src, dict, level)` | Compress with raw dict bytes |
| `decompressUsingDict(alloc, src, dict)` | Decompress with raw dict bytes |
| `compressUsingCDict(alloc, src, cdict)` | Compress with CDict |
| `decompressUsingDDict(alloc, src, ddict)` | Decompress with DDict |
| `getDictIDFromDict(dict)` | Get dict ID from dict bytes |
| `getDictIDFromFrame(src)` | Get dict ID from compressed frame |
| `trainFromSamples(buf, sizes, cap)` | Train dictionary from samples |
| `finalizeDictionary(...)` | Finalize a dictionary |
