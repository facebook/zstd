---
title: Decompressor
description: Reusable decompression context.
---

# Decompressor

A reusable decompression context for decompressing multiple buffers.

## Definition

```zig
pub const Decompressor = struct {
    allocator: std.mem.Allocator,
    // ...
};
```

## Methods

### `init`

Create a new decompressor.

```zig
pub fn init(allocator: std.mem.Allocator, opts: DecompressOptions) Decompressor
```

```zig
var decomp = zstd.Decompressor.init(allocator, .{});
defer decomp.deinit();
```

### `deinit`

Release resources.

```zig
pub fn deinit(self: *Decompressor) void
```

### `decompress`

Decompress data.

```zig
pub fn decompress(self: *Decompressor, src: []const u8) ZstdError![]u8
```

```zig
const decompressed = try decomp.decompress(compressed);
defer allocator.free(decompressed);
```

## Example

```zig
var decomp = zstd.Decompressor.init(allocator, .{});
defer decomp.deinit();

// Decompress multiple buffers
const d1 = try decomp.decompress(c1);
defer allocator.free(d1);

const d2 = try decomp.decompress(c2);
defer allocator.free(d2);
```
