---
title: Frame
description: Inspect zstd frame metadata without decompressing.
---

# Frame

Functions to inspect zstd frame headers without decompressing the data.

## `isFrame`

Check if data starts with a valid zstd frame magic number.

```zig
pub fn isFrame(src: []const u8) bool
```

```zig
if (zstd.Frame.isFrame(data)) {
    std.debug.print("Valid zstd frame\n", .{});
}
```

## `contentSize`

Get the original content size from the frame header.

```zig
pub fn contentSize(src: []const u8) ContentSizeResult
```

```zig
const result = zstd.Frame.contentSize(compressed);
switch (result) {
    .known => |size| std.debug.print("Size: {d}\n", .{size}),
    .unknown => std.debug.print("Unknown\n", .{}),
    .@"error" => std.debug.print("Invalid\n", .{}),
}
```

## `compressedSize`

Get the total compressed size of the frame (including headers and blocks).

```zig
pub fn compressedSize(src: []const u8) ZstdError!usize
```

```zig
const size = try zstd.Frame.compressedSize(compressed);
```

## `dictId`

Get the dictionary ID from the frame header.

```zig
pub fn dictId(src: []const u8) u32
```

```zig
const dict_id = zstd.Frame.dictId(compressed);
// Returns 0 if no dictionary ID is present
```

## ContentSizeResult

```zig
pub const ContentSizeResult = union(enum) {
    known: u64,
    unknown,
    @"error",
};
```
