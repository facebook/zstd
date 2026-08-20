---
title: Frame Inspection
description: Inspect zstd frame metadata without decompressing.
---

# Frame Inspection

## Check if Data is a Zstd Frame

```zig
const zstd = @import("zstd");

if (zstd.Frame.isFrame(data)) {
    std.debug.print("Valid zstd frame\n", .{});
} else {
    std.debug.print("Not a zstd frame\n", .{});
}
```

## Get Content Size

```zig
const result = zstd.Frame.contentSize(compressed);
switch (result) {
    .known => |size| {
        std.debug.print("Original size: {d} bytes\n", .{size});
    },
    .unknown => {
        std.debug.print("Content size not specified in frame\n", .{});
    },
    .@"error" => {
        std.debug.print("Invalid frame header\n", .{});
    },
}
```

## Get Compressed Size

```zig
const size = try zstd.Frame.compressedSize(compressed);
std.debug.print("Compressed frame: {d} bytes\n", .{size});
```

## Get Dictionary ID

```zig
const dict_id = zstd.Frame.dictId(compressed);
if (dict_id != 0) {
    std.debug.print("Dictionary ID: {d}\n", .{dict_id});
}
```

## Complete Frame Inspection

```zig
fn inspectFrame(data: []const u8) void {
    if (!zstd.Frame.isFrame(data)) {
        std.debug.print("Not a zstd frame\n", .{});
        return;
    }

    std.debug.print("Valid zstd frame\n", .{});

    const size = zstd.Frame.contentSize(data);
    switch (size) {
        .known => |s| std.debug.print("  Content size: {d} bytes\n", .{s}),
        .unknown => std.debug.print("  Content size: unknown\n", .{}),
        .@"error" => std.debug.print("  Content size: error\n", .{}),
    }

    const compressed = zstd.Frame.compressedSize(data) catch return;
    std.debug.print("  Compressed size: {d} bytes\n", .{compressed});

    const dict_id = zstd.Frame.dictId(data);
    if (dict_id != 0) {
        std.debug.print("  Dictionary ID: {d}\n", .{dict_id});
    }
}
```
