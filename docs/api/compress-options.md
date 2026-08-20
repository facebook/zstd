---
title: CompressOptions
description: Options struct for zstd.compress().
---

# CompressOptions

Options for the `compress` function. All fields have defaults, so you can use `.{}` for default options.

## Definition

```zig
pub const CompressOptions = struct {
    level: CLevel = .default,
    checksum: bool = false,
    dict_id: u32 = 0,
    use_dict_id: bool = false,
    strategy: ?Strategy = null,
    window_log: ?u32 = null,
};
```

## Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `level` | `CLevel` | `.default` | Compression level (1-22, or named enum) |
| `checksum` | `bool` | `false` | Enable XXH64 frame checksum |
| `dict_id` | `u32` | `0` | Dictionary ID for framed format |
| `use_dict_id` | `bool` | `false` | Whether to include dict_id in frame |
| `strategy` | `?Strategy` | `null` | Compression strategy override |
| `window_log` | `?u32` | `null` | Window log size override |

## Usage

```zig
const zstd = @import("zstd");

// Default options
const c1 = try zstd.compress(allocator, data, .{});

// Custom level
const c2 = try zstd.compress(allocator, data, .{ .level = .fastest });

// With checksum
const c3 = try zstd.compress(allocator, data, .{
    .level = .best,
    .checksum = true,
});

// All options
const c4 = try zstd.compress(allocator, data, .{
    .level = @enumFromInt(12),
    .checksum = true,
    .dict_id = 42,
    .use_dict_id = true,
    .window_log = 22,
});
```
