---
title: Compressor
description: Reusable compression context with parameter control.
---

# Compressor

A reusable compression context. Create once, compress multiple buffers with the same settings.

## Definition

```zig
pub const Compressor = struct {
    level: i32,
    checksum: bool,
    dict_id: u32,
    use_dict_id: bool,
    pledged_src_size: ?u64,
    // ...
};
```

## Methods

### `init`

Create a new compressor.

```zig
pub fn init(opts: CompressOptions) Compressor
```

```zig
var comp = zstd.Compressor.init(.{ .level = .fastest });
defer comp.deinit();
```

### `deinit`

Release resources.

```zig
pub fn deinit(self: *Compressor) void
```

### `compressAlloc`

Compress with allocator (convenience method).

```zig
pub fn compressAlloc(self: *Compressor, allocator: std.mem.Allocator, src: []const u8) ZstdError![]u8
```

```zig
const compressed = try comp.compressAlloc(allocator, data);
defer allocator.free(compressed);
```

### `compress2`

Compress into a pre-allocated buffer.

```zig
pub fn compress2(self: *Compressor, dst: []u8, src: []const u8) ZstdError!usize
```

```zig
var buf: [4096]u8 = undefined;
const written = try comp.compress2(&buf, data);
```

### `setParameter`

Change a compression parameter.

```zig
pub fn setParameter(self: *Compressor, param: CParameter, value: i32) ZstdError!void
```

```zig
try comp.setParameter(.compression_level, 5);
try comp.setParameter(.checksum_flag, 1);
```

### `reset`

Reset the compressor state.

```zig
pub fn reset(self: *Compressor, directive: ResetDirective) ZstdError!void
```

```zig
// Reset parameters only
try comp.reset(.parameters);

// Reset everything
try comp.reset(.session_and_parameters);
```

### `setPledgedSrcSize`

Set the content size for the frame header.

```zig
pub fn setPledgedSrcSize(self: *Compressor, src_size: u64) ZstdError!void
```

## Example

```zig
var comp = zstd.Compressor.init(.{ .level = .default });
defer comp.deinit();

// First compression
const c1 = try comp.compressAlloc(allocator, data1);
defer allocator.free(c1);

// Change level and compress again
try comp.setParameter(.compression_level, 9);
const c2 = try comp.compressAlloc(allocator, data2);
defer allocator.free(c2);
```
