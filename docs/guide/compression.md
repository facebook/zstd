---
title: Compression
description: Compress data with zstd.zig using options structs and compression levels.
---

# Compression

## One-Shot Compression

The simplest way to compress data:

```zig
const zstd = @import("zstd");

const compressed = try zstd.compress(allocator, data, .{});
defer allocator.free(compressed);
```

## Compression Levels

zstd.zig provides a `CLevel` enum for common compression levels:

| Level | Value | Description |
|-------|-------|-------------|
| `.fastest` | 1 | Fastest compression, lower ratio |
| `.default` | 3 | Balanced speed and ratio |
| `.best` | 19 | Best compression ratio, slower |

```zig
// Named levels
const fast = try zstd.compress(allocator, data, .{ .level = .fastest });
const balanced = try zstd.compress(allocator, data, .{ .level = .default });
const small = try zstd.compress(allocator, data, .{ .level = .best });

// Custom level (integer)
const custom = try zstd.compress(allocator, data, .{
    .level = @enumFromInt(12),
});
```

## CompressOptions

All available options:

```zig
const compressed = try zstd.compress(allocator, data, .{
    .level = .default,      // CLevel enum or @enumFromInt(n)
    .checksum = false,      // Enable XXH64 checksum
    .dict_id = 0,           // Dictionary ID for framed format
    .use_dict_id = false,   // Whether to include dict_id in frame
    .strategy = null,       // Compression strategy override
    .window_log = null,     // Window log size override
});
```

## Reusable Compressor

For compressing multiple buffers with the same settings:

```zig
var comp = zstd.Compressor.init(.{
    .level = .default,
    .checksum = true,
});
defer comp.deinit();

// Compress buffer 1
const c1 = try comp.compressAlloc(allocator, data1);
defer allocator.free(c1);

// Compress buffer 2
const c2 = try comp.compressAlloc(allocator, data2);
defer allocator.free(c2);
```

### Compressor Methods

| Method | Description |
|--------|-------------|
| `init(opts)` | Create a new compressor |
| `deinit()` | Release resources |
| `compress2(dst, src)` | Compress into pre-allocated buffer |
| `compressAlloc(alloc, src)` | Compress with allocator |
| `setParameter(param, value)` | Change a parameter |
| `reset(directive)` | Reset compressor state |
| `setPledgedSrcSize(size)` | Set content size for frame header |

### Parameters

```zig
try comp.setParameter(.compression_level, 5);
try comp.setParameter(.checksum_flag, 1);
try comp.setParameter(.window_log, 22);
```

## compressBound

Get the maximum compressed size for a given input size:

```zig
const bound = try zstd.compressBound(src.len);
// bound >= src.len is guaranteed
```

## Checksum

Enable frame checksum for data integrity verification:

```zig
const compressed = try zstd.compress(allocator, data, .{
    .checksum = true,
});
// The decompressor will verify the checksum automatically
```
