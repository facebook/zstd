---
title: Constants
description: Library constants for magic numbers, sizes, and limits.
---

# Constants

All library constants are available under the `zstd.constants` namespace.

## Magic Numbers

| Constant | Value | Description |
|----------|-------|-------------|
| `magic_number` | `0xFD2FB528` | Zstandard frame magic number |
| `magic_dictionary` | `0xEC30A437` | Dictionary format magic number |
| `magic_skippable_start` | `0x184D2A50` | Skippable frame start magic |
| `magic_skippable_mask` | `0xFFFFFFF0` | Mask for skippable frame detection |

## Sizes and Limits

| Constant | Value | Description |
|----------|-------|-------------|
| `block_size_max` | `131072` (128 KB) | Maximum block size |
| `block_size_log_max` | `17` | Log2 of maximum block size |
| `content_size_unknown` | `0xFFFFFFFFFFFFFFFF` | Sentinel for unknown content size |
| `content_size_error` | `0xFFFFFFFFFFFFFFFE` | Sentinel for content size error |
| `max_input_size` | platform-dependent | Maximum input size |

## Usage

```zig
const zstd = @import("zstd");

// Check magic number
if (data.len >= 4) {
    const magic = std.mem.readInt(u32, data[0..4], .little);
    if (magic == zstd.constants.magic_number) {
        // It's a zstd frame
    }
}

// Maximum block size
const max_block = zstd.constants.block_size_max; // 131072
```

## Version Information

Available under the `zstd.version` namespace:

| Constant | Value |
|----------|-------|
| `number` | `10600` |
| `string` | `"1.6.0"` |
| `major` | `1` |
| `minor` | `6` |
| `release` | `0` |
| `clevel_default` | `3` |
| `clevel_min` | `-131072` |
| `clevel_max` | `22` |

```zig
const zstd = @import("zstd");

std.debug.print("Version: {s}\n", .{zstd.version.string});
std.debug.print("Number: {d}\n", .{zstd.version.number});
```
