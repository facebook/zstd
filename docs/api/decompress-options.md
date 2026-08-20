---
title: DecompressOptions
description: Options struct for zstd.decompress().
---

# DecompressOptions

Options for the `decompress` function.

## Definition

```zig
pub const DecompressOptions = struct {
    dict: ?[]const u8 = null,
};
```

## Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `dict` | `?[]const u8` | `null` | Optional dictionary data for decompression |

## Usage

```zig
const zstd = @import("zstd");

// Default options
const d1 = try zstd.decompress(allocator, compressed, .{});

// With dictionary
const d2 = try zstd.decompress(allocator, compressed, .{
    .dict = dict_data,
});
```
