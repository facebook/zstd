---
title: Decompression
description: Decompress data with zstd.zig and inspect frame metadata.
---

# Decompression

## One-Shot Decompression

```zig
const zstd = @import("zstd");

const decompressed = try zstd.decompress(allocator, compressed, .{});
defer allocator.free(decompressed);
```

## DecompressOptions

```zig
const decompressed = try zstd.decompress(allocator, compressed, .{
    .dict = null,  // Optional dictionary data
});
```

## Reusable Decompressor

```zig
var decomp = zstd.Decompressor.init(allocator, .{});
defer decomp.deinit();

const d1 = try decomp.decompress(c1);
defer allocator.free(d1);

const d2 = try decomp.decompress(c2);
defer allocator.free(d2);
```

## Frame Inspection

Inspect zstd frame metadata without decompressing:

### Check if data is a zstd frame

```zig
if (zstd.Frame.isFrame(data)) {
    std.debug.print("Valid zstd frame\n", .{});
}
```

### Get original content size

```zig
const result = zstd.Frame.contentSize(compressed);
switch (result) {
    .known => |size| std.debug.print("Content size: {d}\n", .{size}),
    .unknown => std.debug.print("Content size unknown\n", .{}),
    .@"error" => std.debug.print("Invalid frame\n", .{}),
}
```

### Get compressed frame size

```zig
const size = try zstd.Frame.compressedSize(compressed);
std.debug.print("Frame size: {d} bytes\n", .{size});
```

### Get dictionary ID

```zig
const dict_id = zstd.Frame.dictId(compressed);
if (dict_id != 0) {
    std.debug.print("Dictionary ID: {d}\n", .{dict_id});
}
```

## Error Handling

Decompression can fail with various errors:

```zig
const decompressed = zstd.decompress(allocator, data, .{}) catch |err| {
    switch (err) {
        error.PrefixUnknown => std.debug.print("Not a zstd frame\n", .{}),
        error.CorruptionDetected => std.debug.print("Data corrupted\n", .{}),
        error.SrcSizeWrong => std.debug.print("Source too short\n", .{}),
        else => std.debug.print("Error: {}\n", .{err}),
    }
    return err;
};
```
