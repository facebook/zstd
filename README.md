<div align="center">

# zstd.zig

<a href="https://ziglang.org/"><img src="https://img.shields.io/badge/Zig-0.16.0-orange.svg?logo=zig" alt="Zig Version"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig"><img src="https://img.shields.io/github/stars/muhammad-fiaz/zstd.zig" alt="GitHub stars"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig/issues"><img src="https://img.shields.io/github/issues/muhammad-fiaz/zstd.zig" alt="GitHub issues"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig/pulls"><img src="https://img.shields.io/github/issues-pr/muhammad-fiaz/zstd.zig" alt="GitHub pull requests"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig"><img src="https://img.shields.io/github/last-commit/muhammad-fiaz/zstd.zig" alt="GitHub last commit"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig/blob/dev/LICENSE"><img src="https://img.shields.io/badge/License-BSD%20%2B%20GPLv2-blue.svg" alt="License"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig/actions/workflows/ci.yml"><img src="https://github.com/muhammad-fiaz/zstd.zig/actions/workflows/ci.yml/badge.svg?branch=dev" alt="CI"></a>
<img src="https://img.shields.io/badge/platforms-linux%20%7C%20windows%20%7C%20macos-blue" alt="Supported Platforms">
<a href="https://github.com/muhammad-fiaz/zstd.zig/releases/latest"><img src="https://img.shields.io/github/v/release/muhammad-fiaz/zstd.zig?label=Latest%20Release&style=flat-square" alt="Latest Release"></a>
<a href="https://hits.sh/muhammad-fiaz/zstd.zig/"><img src="https://hits.sh/muhammad-fiaz/zstd.zig.svg?label=Visitors&extraCount=0&color=green" alt="Repo Visitors"></a>

<p><em>High-level native Zig bindings for Facebook's Zstandard fast compression library.</em></p>

<b><a href="#installation">Installation</a> |
<a href="#quick-start">Quick Start</a> |
<a href="#feature-coverage">Features</a> |
<a href="#api-reference">API</a> |
<a href="#license">License</a></b>

</div>

---

High-level native Zig bindings for Facebook's [zstd](https://github.com/facebook/zstd) (Zstandard) fast compression library. Wraps the full zstd C API (`zstd.h`, `zstd_errors.h`, `zdict.h`) into idiomatic, safe, well-documented Zig modules.

> [!NOTE]
> These bindings track upstream Facebook zstd commit [`5c7b7bad`](https://github.com/facebook/zstd/commit/5c7b7bad26808e6b40ac3b3d0075466e27738a9d) (v1.6.0).

## Requirements

> [!NOTE]
> * Zig 0.16.0 or later is required.

## Installation

### Option 1: Stable Release (Recommended)

Install the latest stable release from the official release archive:

```bash
zig fetch --save https://github.com/muhammad-fiaz/zstd.zig/archive/refs/tags/0.0.1.tar.gz
```

Or add it directly to your `build.zig.zon`:

```zig
.dependencies = .{
    .zstd = .{
        .url = "https://github.com/muhammad-fiaz/zstd.zig/archive/refs/tags/0.0.1.tar.gz",
        .hash = "...", // Run `zig build` to obtain the hash
    },
},
```

---

### Option 2: Nightly

Track the latest development version from the repository:

```bash
zig fetch --save git+https://github.com/muhammad-fiaz/zstd.zig.git
```

Or pin to a specific commit:

```bash
zig fetch --save git+https://github.com/muhammad-fiaz/zstd.zig.git#COMMIT_HASH
```

Or add it to your `build.zig.zon`:

```zig
.dependencies = .{
    .zstd = .{
        .url = "git+https://github.com/muhammad-fiaz/zstd.zig.git#COMMIT_HASH",
        .hash = "...", // Run `zig build` to obtain the hash
    },
},
```

---

### Option 3: Local Path

Clone the repository:

```bash
git clone https://github.com/muhammad-fiaz/zstd.zig.git
```

Then reference it from your `build.zig.zon`:

```zig
.dependencies = .{
    .zstd = .{
        .path = "/path/to/zstd.zig",
    },
},
```

---

### Importing

After adding the dependency, import the module in your `build.zig`:

```zig
const zstd_dep = b.dependency("zstd", .{});
exe.root_module.addImport("zstd", zstd_dep.module("zstd"));
```

Then use it in your Zig source:

```zig
const zstd = @import("zstd");
```

## Quick Start

### Compress and Decompress

```zig
const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const data = "Hello, Zstandard from Zig!";

    // Compress
    const compressed = try zstd.compress(std.heap.c_allocator, data, 3);
    defer std.heap.c_allocator.free(compressed);

    // Decompress
    const decompressed = try zstd.decompress(std.heap.c_allocator, compressed, data.len);
    defer std.heap.c_allocator.free(decompressed);

    std.debug.print("Compressed {d} -> {d} bytes\n", .{ data.len, compressed.len });
}
```

### Streaming Compression

```zig
var scomp = try zstd.StreamingCompressor.init();
defer scomp.deinit();

try scomp.setParameter(.compression_level, 6);

var output: [4096]u8 = undefined;
const res = try scomp.compressChunk(data, &output, .end);
const compressed_data = output[0..res.bytes_written];
```

### Dictionary Compression

```zig
// Train a dictionary from sample data
const dict = try zstd.zdict.trainFromSamples(allocator, samples, 1 << 14, .{});
defer allocator.free(dict);

// Compress with dictionary
const compressed = try zstd.zdict.compressUsingDict(allocator, data, dict, 3);
defer allocator.free(compressed);
```

## Feature Coverage

| Feature | Supported | Zig Entry Point |
|---|---|---|
| One-shot Compress | Yes | `zstd.compress` |
| One-shot Decompress | Yes | `zstd.decompress` |
| Compress Bound | Yes | `zstd.compressBound` |
| Frame Content Size | Yes | `zstd.getFrameContentSize` |
| Frame Detection | Yes | `zstd.isFrame` |
| Streaming Compress | Yes | `zstd.StreamingCompressor` |
| Streaming Decompress | Yes | `zstd.StreamingDecompressor` |
| Compression Context (CCtx) | Yes | `zstd.Compressor` |
| Decompression Context (DCtx) | Yes | `zstd.Decompressor` |
| Compression Parameters | Yes | `zstd.CParameter` (13 variants) |
| Decompression Parameters | Yes | `zstd.DParameter` |
| Compression Strategies | Yes | `zstd.Strategy` (9 levels) |
| CDict (Prepared Dict) | Yes | `zstd.CDict` |
| DDict (Prepared Dict) | Yes | `zstd.DDict` |
| Dictionary Builder (ZDICT) | Yes | `zstd.zdict.trainFromSamples` |
| Finalize Dictionary | Yes | `zstd.zdict.finalizeDictionary` |
| Multithreaded Compression | Yes | `CParameter.nb_workers` |
| Legacy Format Support | Yes | Build option `-Dlegacy=true` |
| Custom Allocator | Yes | All `init`/`compress`/`decompress` accept `std.mem.Allocator` |
| Error Handling | Yes | `zstd.ZstdError` (30+ error codes) |
| Version Query | Yes | `zstd.versionString` / `zstd.versionNumber` |

## Usage

See the [examples/](examples/) directory for complete, runnable programs.

### Streaming Example

```zig
var scomp = try zstd.StreamingCompressor.init();
defer scomp.deinit();

try scomp.setParameter(.compression_level, 6);

var comp_buf = try allocator.alloc(u8, zstd.stream.cStreamOutSize());
defer allocator.free(comp_buf);

// Feed data in chunks
const res = try scomp.compressChunk(chunk, comp_buf, .@"continue");
if (res.bytes_written > 0) {
    // Write comp_buf[0..res.bytes_written] to output
}

// End the frame
const final = try scomp.endStream(comp_buf);
if (final.bytes_written > 0) {
    // Write comp_buf[0..final.bytes_written]
}
```

### Compression Context Example

```zig
var comp = try zstd.Compressor.init();
defer comp.deinit();

try comp.setParameter(.compression_level, 10);
try comp.setParameter(.nb_workers, 4);
try comp.setParameter(.checksum_flag, 1);

const compressed = try comp.compressAlloc(allocator, data);
defer allocator.free(compressed);
```

### Dictionary Training Example

```zig
const samples = [_][]const u8{
    "sample one for training",
    "sample two for training",
    "sample three for training",
};

// Train a 16 KB dictionary
const dict = try zstd.zdict.trainFromSamples(allocator, &samples, 1 << 14, .{});
defer allocator.free(dict);
```

## API Reference

### Single-shot

| Function | Signature | Description |
|---|---|---|
| `zstd.compress` | `(allocator, src, level) ZstdError![]u8` | Compress data in one shot |
| `zstd.decompress` | `(allocator, src, expected_size) ZstdError![]u8` | Decompress data in one shot |
| `zstd.compressBound` | `(src_size) ZstdError!usize` | Worst-case compressed size |
| `zstd.getFrameContentSize` | `(src) ContentSizeResult` | Decompressed size from frame header |
| `zstd.findFrameCompressedSize` | `(src) ZstdError!usize` | Compressed size of first frame |
| `zstd.isFrame` | `(src) bool` | Check for valid zstd frame magic |

### Streaming

| Type | Description |
|---|---|
| `zstd.StreamingCompressor` | Chunk by chunk compression for large data |
| `zstd.StreamingDecompressor` | Chunk by chunk decompression for large data |
| `zstd.StreamResult` | Result with `bytes_written` and `remaining` fields |
| `zstd.EndDirective` | `.continue`, `.flush`, or `.end` |

### Compression Context

| Type | Description |
|---|---|
| `zstd.Compressor` | Explicit CCtx with full parameter control |
| `zstd.CParameter` | 13 compression parameters (level, window, strategy, threads, etc.) |
| `zstd.Strategy` | 9 strategies from `fast` to `btultra2` |
| `zstd.ResetDirective` | `session_only`, `parameters`, `session_and_parameters` |

### Decompression Context

| Type | Description |
|---|---|
| `zstd.Decompressor` | Explicit DCtx with full parameter control |
| `zstd.DParameter` | Decompression parameters |

### Dictionary

| Type | Description |
|---|---|
| `zstd.CDict` | Prepared compression dictionary |
| `zstd.DDict` | Prepared decompression dictionary |
| `zstd.zdict.trainFromSamples` | Train dictionary from sample data |
| `zstd.zdict.finalizeDictionary` | Finalize a custom dictionary |

### Error Handling

| Error | Description |
|---|---|
| `error.CorruptionDetected` | Data corruption detected |
| `error.DstSizeTooSmall` | Destination buffer too small |
| `error.MemoryAllocation` | C memory allocation failed |
| `error.ParameterOutOfBound` | Parameter out of valid range |
| ... | 30+ errors total, see `errors.zig` |

## Building & Testing

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

## API Modules

| Module | Description |
|---|---|
| `zstd` | Public root module with re-exported convenience functions |
| `zstd.simple` | One-shot compress/decompress |
| `zstd.cctx` | Compression context, CParameter, Strategy |
| `zstd.dctx` | Decompression context, DParameter |
| `zstd.stream` | StreamingCompressor, StreamingDecompressor |
| `zstd.dict` | Dictionary compression with CDict/DDict |
| `zstd.zdict` | Dictionary builder (ZDICT) |
| `zstd.errors` | ZstdError error set and error code mapping |
| `zstd.version` | Version and capability queries |

## Contributing

Contributions are welcome! Please open an issue or pull request on GitHub.

## License

BSD + GPLv2 - see [LICENSE](LICENSE) and [COPYING](COPYING).

Original zstd code: Copyright (c) Meta Platforms, Inc. and affiliates.

Zig bindings: Copyright (c) Muhammad Fiaz.

## Author

**Muhammad Fiaz** (https://github.com/muhammad-fiaz) - Zig bindings, build system, and API design
