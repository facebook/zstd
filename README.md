# zstd.zig

Zig bindings for Facebook's [zstd](https://github.com/facebook/zstd) (Zstandard) fast compression library. Wraps the full zstd C API (`zstd.h`, `zstd_errors.h`, `zdict.h`) into idiomatic, safe, well-documented Zig modules.

**Zig version:** 0.16.0 or later
**zstd C library version:** 1.6.0 (commit [`5c7b7bad`](https://github.com/facebook/zstd/commit/5c7b7bad26808e6b40ac3b3d0075466e27738a9d))
**Binding version:** 0.0.1

## Quick Start

```zig
const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const data = "Hello, Zstandard from Zig!";

    const compressed = try zstd.compress(std.heap.c_allocator, data, 3);
    defer std.heap.c_allocator.free(compressed);

    const decompressed = try zstd.decompress(std.heap.c_allocator, compressed, data.len);
    defer std.heap.c_allocator.free(decompressed);

    std.debug.print("Compressed {d} -> {d} bytes\n", .{ data.len, compressed.len });
}
```

## Requirements

* Zig 0.16.0 or later

## Installation

### Option 1: Stable Release (Tag-based, Recommended)

Use a version tag for stable, reproducible builds:

```bash
zig fetch --save git+https://github.com/muhammad-fiaz/zstd.zig.git#0.0.1
```

Or add to your `build.zig.zon`:

```zig
.dependencies = .{
    .zstd = .{
        .url = "git+https://github.com/muhammad-fiaz/zstd.zig.git#0.0.1",
        .hash = "...", // Run zig build to obtain
    },
},
```

### Option 2: Nightly (Latest Commit)

Track the latest development changes:

```bash
zig fetch --save git+https://github.com/muhammad-fiaz/zstd.zig.git
```

Or pin to a specific commit:

```zig
.dependencies = .{
    .zstd = .{
        .url = "git+https://github.com/muhammad-fiaz/zstd.zig.git#5c7b7bad",
        .hash = "...",
    },
},
```

### Option 3: Local Path

Clone and use as a local dependency:

```bash
git clone https://github.com/muhammad-fiaz/zstd.zig.git
```

```zig
.dependencies = .{
    .zstd = .{
        .path = "/path/to/zstd.zig",
    },
},
```

### In your build.zig

```zig
const zstd_dep = b.dependency("zstd", .{});
exe.root_module.addImport("zstd", zstd_dep.module("zstd"));
```

## API

All functions are available directly from `zstd`:

```zig
const zstd = @import("zstd");
```

### Single-shot

| Function | Description |
|---|---|
| `zstd.compress(allocator, src, level)` | Compress data in one shot |
| `zstd.decompress(allocator, src, expected_size)` | Decompress data in one shot |
| `zstd.compressBound(src_size)` | Worst-case compressed size |
| `zstd.getFrameContentSize(src)` | Decompressed size from frame header |
| `zstd.findFrameCompressedSize(src)` | Compressed size of first frame |
| `zstd.isFrame(src)` | Check for valid zstd frame magic |

### Streaming

| Type | Description |
|---|---|
| `zstd.StreamingCompressor` | Chunk by chunk compression for large data |
| `zstd.StreamingDecompressor` | Chunk by chunk decompression for large data |

### Compression Context

| Type | Description |
|---|---|
| `zstd.Compressor` | Explicit CCtx with full parameter control |
| `zstd.Decompressor` | Explicit DCtx with full parameter control |
| `zstd.CDict` / `zstd.DDict` | Pre-loaded dictionaries for repeated use |

### Dictionary Builder

| Function | Description |
|---|---|
| `zstd.zdict.trainFromSamples()` | Train a dictionary from sample data |
| `zstd.zdict.finalizeDictionary()` | Finalize a dictionary from raw content |

## Building

```bash
zig build            # Build library
zig build test       # Run unit tests
zig build docs       # Generate documentation (zig-out/docs/)
zig build example-simple-compress   # Run an example
```

### Build Options

| Option | Default | Description |
|---|---|---|
| `-Dmultithread` | `true` | Enable multithreaded compression (ZSTD_MULTITHREAD) |
| `-Dlegacy` | `false` | Enable legacy format decoding (v0.1 to v0.7) |

## Examples

Run any example with `zig build example-<name>`. See [examples/README.md](examples/README.md) for the full list.

```bash
zig build example-simple-compress
zig build example-streaming-compress-file
zig build example-dictionary-training
zig build example-benchmark-levels
```

## Platform Support

* Windows, macOS, Linux
* x86_64, aarch64, and other targets supported by Zig and zstd

## Authors

* **Muhammad Fiaz** (https://github.com/muhammad-fiaz) - Zig bindings, build system, and API design
* **Meta Platforms (Facebook)** (https://github.com/facebook/zstd) - Original zstd C library

## License

BSD + GPLv2 (see [LICENSE](LICENSE) and [COPYING](COPYING)).

Original zstd code: Copyright (c) Meta Platforms, Inc. and affiliates.

Zig bindings: Copyright (c) Muhammad Fiaz.
