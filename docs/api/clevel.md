---
title: CLevel
description: Compression level enum with named values and raw integer support.
---

# CLevel

Compression level enum that supports both named levels and raw integers.

## Definition

```zig
pub const CLevel = enum(i32) {
    fastest = 1,
    default = 3,
    best = 19,
    _,
    // ...
};
```

## Named Levels

| Value | Integer | Description |
|-------|---------|-------------|
| `.fastest` | 1 | Fastest compression, lower ratio |
| `.default` | 3 | Balanced speed and ratio |
| `.best` | 19 | Best compression ratio, slower |

## Methods

### `toInt`

Convert to integer.

```zig
pub fn toInt(self: CLevel) i32
```

```zig
const val = CLevel.fastest.toInt(); // 1
```

### `fromInt`

Create from integer.

```zig
pub fn fromInt(val: i32) CLevel
```

```zig
const level = CLevel.fromInt(12);
```

## Usage

```zig
// Named enum values
const c1 = try zstd.compress(allocator, data, .{ .level = .fastest });
const c2 = try zstd.compress(allocator, data, .{ .level = .default });
const c3 = try zstd.compress(allocator, data, .{ .level = .best });

// Custom level via @enumFromInt
const c4 = try zstd.compress(allocator, data, .{
    .level = @enumFromInt(7),
});

// In Compressor
var comp = zstd.Compressor.init(.{ .level = .best });
```

## Range

The full range is -131072 to 22, matching the C zstd library.
