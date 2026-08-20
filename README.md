<div align="center">

<a href="https://muhammad-fiaz.github.io/zstd.zig/"><img src="https://img.shields.io/badge/docs-muhammad--fiaz.github.io-blue" alt="Documentation"></a>
<a href="https://ziglang.org/"><img src="https://img.shields.io/badge/Zig-0.17.0--dev-orange.svg?logo=zig" alt="Zig Version"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig"><img src="https://img.shields.io/github/stars/muhammad-fiaz/zstd.zig" alt="GitHub stars"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig/issues"><img src="https://img.shields.io/github/issues/muhammad-fiaz/zstd.zig" alt="GitHub issues"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig/pulls"><img src="https://img.shields.io/github/issues-pr/muhammad-fiaz/zstd.zig" alt="GitHub pull requests"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig"><img src="https://img.shields.io/github/last-commit/muhammad-fiaz/zstd.zig" alt="GitHub last commit"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig/blob/dev/LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License"></a>
<a href="https://github.com/muhammad-fiaz/zstd.zig/actions/workflows/ci.yml"><img src="https://github.com/muhammad-fiaz/zstd.zig/actions/workflows/ci.yml/badge.svg?branch=dev" alt="CI"></a>
<img src="https://img.shields.io/badge/platforms-linux%20%7C%20windows%20%7C%20macos%20%7C%20freebsd-blue" alt="Supported Platforms">
<a href="https://github.com/muhammad-fiaz/zstd.zig/releases/latest"><img src="https://img.shields.io/github/v/release/muhammad-fiaz/zstd.zig?label=Latest%20Release&style=flat-square" alt="Latest Release"></a>
<a href="https://pay.muhammadfiaz.com"><img src="https://img.shields.io/badge/Sponsor-pay.muhammadfiaz.com-ff69b4?style=flat&logo=heart" alt="Sponsor"></a>
<a href="https://github.com/sponsors/muhammad-fiaz"><img src="https://img.shields.io/badge/Sponsor-GitHub-pink?style=social&logo=github" alt="GitHub Sponsors"></a>
<a href="https://hits.sh/muhammad-fiaz/zstd.zig/"><img src="https://hits.sh/muhammad-fiaz/zstd.zig.svg?label=Visitors&extraCount=0&color=green" alt="Repo Visitors"></a>

<p><em>Complete native Zig implementation of Facebook's Zstandard fast compression library.</em></p>

<b><a href="https://muhammad-fiaz.github.io/zstd.zig/">Documentation</a> |
<a href="https://muhammad-fiaz.github.io/zstd.zig/api/">API Reference</a> |
<a href="https://muhammad-fiaz.github.io/zstd.zig/guide/getting-started">Quick Start</a> |
<a href="CONTRIBUTING.md">Contributing</a></b>

</div>

`zstd.zig` is a complete native Zig implementation of Facebook's [Zstandard](https://github.com/facebook/zstd) fast compression library. No C bindings, no external dependencies. Every byte is Zig.

> [!TIP]
> If you build with zstd.zig, make sure to give it a star.

> [!NOTE]
> **Project maturity:** This project provides a native Zig implementation of Zstandard compression. It supports one-shot and streaming compression/decompression, dictionary-based compression, frame inspection, and parameter configuration. The implementation is actively evolving.
>
> **Pure Zig — zero C dependencies:** Unlike binding-based approaches, `zstd.zig` implements the Zstandard format directly in Zig, including:
> - **Frame format** with magic number validation, content size detection, and checksum verification
> - **Block structure** with raw, RLE, and compressed block types
> - **Huffman coding** for literal compression and decompression
> - **FSE (Finite State Entropy)** table construction and decoding for sequence compression
> - **LZ77** back-reference matching for sliding window compression
> - **Dictionary support** with CDict/DDict for trained dictionaries and dictionary-based compression
> - **Streaming API** with StreamCompressor/StreamDecompressor for chunked data processing
> - **Parameter API** for fine-tuning compression level, window size, hash tables, and strategies
> - **Frame inspection** for metadata extraction without full decompression

---

<details>
<summary><strong>Features</strong> (click to expand)</summary>

| Feature | Description | Status |
|---------|-------------|--------|
| **One-shot Compression** | `zstd.compress()` for single-call compression with configurable options | Implemented |
| **One-shot Decompression** | `zstd.decompress()` for single-call decompression with safety limits | Implemented |
| **Compression Levels** | Named levels (`.fastest`, `.default`, `.best`) and raw numeric levels (1-22) | Implemented |
| **Reusable Compressor** | `Compressor` struct for efficient multi-call compression with state | Implemented |
| **Reusable Decompressor** | `Decompressor` struct for efficient multi-call decompression with configurable limits | Implemented |
| **Streaming Compression** | `StreamCompressor` for chunked data with `compressChunk()` and `endStream()` | Implemented |
| **Streaming Decompression** | `StreamDecompressor` for chunked data with `decompressChunk()` | Implemented |
| **Dictionary Compression** | `CDict`/`DDict` for trained dictionaries with `compress()`/`decompress()` methods | Implemented |
| **Dictionary Training** | `trainFromSamples()` and `finalizeDictionary()` for creating custom dictionaries | Implemented |
| **Frame Inspection** | `Frame.isFrame()`, `Frame.inspect()`, `Frame.contentSize()` for metadata extraction | Implemented |
| **Parameter Bounds** | `cParamGetBounds()` and `dParamGetBounds()` for querying parameter ranges | Implemented |
| **CParameter API** | Compression parameters: window_log, hash_log, chain_log, search_log, min_match, strategy | Implemented |
| **DParameter API** | Decompression parameters: window_log_max | Implemented |
| **Checksum Support** | Optional XXH64 checksum in frame headers for data integrity verification | Implemented |
| **Reserved Bit Rejection** | Strict validation of reserved bits in frame headers and block types | Implemented |
| **Content Size Validation** | Validates content size on decompression against expected size | Implemented |
| **Window Size Limits** | Configurable `max_window_size` for decompression safety | Implemented |
| **Output Size Limits** | Configurable `max_output_size` to prevent unbounded allocation | Implemented |
| **Multi-frame Decompression** | Decompress multiple concatenated zstd frames in sequence | Implemented |
| **Skippable Frame Support** | Skip non-data frames during decompression | Implemented |
| **Cross-platform** | Linux, Windows, macOS, FreeBSD with x86_64, aarch64, x86 support | Implemented |
| **Zero Dependencies** | Pure Zig implementation — no C libraries, no system dependencies | Implemented |
| **Strategy Selection** | Fast, DFast, Greedy, Lazy, Lazy2, BTLazy2, BTOpt, BTUltra strategies | Implemented |
| **Compression Bound** | `compressBound()` for pre-allocating output buffers | Implemented |
| **Backward Compatible** | Legacy function aliases available alongside modern API | Implemented |

</details>

---

<details>
<summary><strong>Prerequisites and Supported Platforms</strong> (click to expand)</summary>

<br>

## Prerequisites

Before using `zstd.zig`, ensure you have the following:

| Requirement | Version | Notes |
|-------------|---------|-------|
| **Zig** | **0.17.0** (development) | Install via `scoop install versions/zig-dev` or download from [ziglang.org](https://ziglang.org/download/) |
| **Operating System** | Windows 10+, Linux, macOS, FreeBSD | Cross-platform support |

> [!IMPORTANT]
> **Zig 0.17.0 is required.** This project targets Zig 0.17.0 (development version). The native Zig implementation requires 0.17.0+ features. Install the development version via:
> ```bash
> # Using Scoop (Windows)
> scoop bucket add versions
> scoop install versions/zig-dev
>
> # Or download from https://ziglang.org/download/
> ```

---

## Supported Platforms

`zstd.zig` is validated on these architectures:

| Platform | x86_64 (64-bit) | aarch64 (ARM64) | x86 (32-bit) |
|----------|-----------------|-----------------|--------------|
| **Linux** | Yes | Yes | Yes |
| **Windows** | Yes | Yes | Yes |
| **macOS** | Yes | Yes (Apple Silicon) | No |
| **FreeBSD** | Yes | Yes | No |

### Cross-Compilation

Zig makes cross-compilation easy. Build for any target from any host:

```bash
# Build for Linux ARM64 from Windows
zig build -Dtarget=aarch64-linux

# Build for Windows from Linux
zig build -Dtarget=x86_64-windows

# Build for macOS Apple Silicon from Linux
zig build -Dtarget=aarch64-macos

# Build for 32-bit Windows
zig build -Dtarget=x86-windows
```

</details>

---

## Installation

### Method 1: Zig Fetch (Recommended)

Fetch the latest version directly:

```bash
zig fetch --save git+https://github.com/muhammad-fiaz/zstd.zig.git
```

### Method 2: Manual `build.zig.zon` Configuration

Add the dependency to your `build.zig.zon` file:

```zig
.dependencies = .{
    .zstd = .{
        .url = "git+https://github.com/muhammad-fiaz/zstd.zig.git",
        .hash = "...", // Run `zig fetch --save <url>` to generate the hash.
    },
},
```

### Method 3: Local Source Checkout

Clone the repository locally.

```bash
git clone https://github.com/muhammad-fiaz/zstd.zig.git
cd zstd.zig
zig build
```

To use a local checkout from another project, add a path dependency to your `build.zig.zon`:

```zig
.dependencies = .{
    .zstd = .{
        .path = "../zstd.zig",
    },
},
```

### Wire into `build.zig`

After adding the dependency, import the module in your `build.zig`:

```zig
const zstd_dep = b.dependency("zstd", .{
    .target = target,
    .optimize = optimize,
});
exe.root_module.addImport("zstd", zstd_dep.module("zstd"));
```

## Quick Start

### One-Liner Compression

```zig
const zstd = @import("zstd");

// Compress — simplest possible usage
const compressed = try zstd.compress(allocator, data, .{});
defer allocator.free(compressed);

// Decompress
const decompressed = try zstd.decompress(allocator, compressed, .{});
defer allocator.free(decompressed);
```

### Compression Levels

```zig
// Named levels
const fast = try zstd.compress(allocator, data, .{ .level = .fastest });
const balanced = try zstd.compress(allocator, data, .{ .level = .default });
const small = try zstd.compress(allocator, data, .{ .level = .best });

// Custom level (1-22)
const custom = try zstd.compress(allocator, data, .{
    .level = zstd.CLevel.fromInt(12),
});
```

### Reusable Contexts

```zig
var comp = zstd.Compressor.init(.{ .level = .default });
defer comp.deinit();

const c1 = try comp.compressAlloc(allocator, data1);
defer allocator.free(c1);

const c2 = try comp.compressAlloc(allocator, data2);
defer allocator.free(c2);
```

### Streaming Compression

```zig
var comp = zstd.StreamCompressor.init(allocator, .{ .level = .default });
defer comp.deinit();

var output: [zstd.recommendedCOutSize()]u8 = undefined;

const r1 = try comp.compressChunk(chunk1, &output, .@"continue");
const r2 = try comp.compressChunk(chunk2, &output, .@"continue");
const final = try comp.endStream(&output);
```

### Streaming Decompression

```zig
var sd = zstd.StreamDecompressor.init(allocator, .{});
defer sd.deinit();

var output: [zstd.recommendedDOutSize()]u8 = undefined;
const result = try sd.decompressChunk(compressed_data, &output);
const text = output[0..result.bytes_written];
```

### Frame Inspection

```zig
if (zstd.Frame.isFrame(data)) {
    const info = zstd.Frame.inspect(data);
    if (info) |i| {
        std.debug.print("Content size: {?}\n", .{i.content_size});
        std.debug.print("Checksum: {}\n", .{i.checksum});
    }
}
```

### Dictionary Compression

```zig
// Create dictionaries from samples
var dict = try zstd.trainFromSamples(allocator, samples_buf, &sizes, 1024);
defer allocator.free(dict);

// Compress with dictionary
var cdict = zstd.CDict.init(dict, 3);
defer cdict.deinit();
const compressed = try cdict.compress(allocator, data);

// Decompress with dictionary
var ddict = zstd.DDict.init(dict);
defer ddict.deinit();
const decompressed = try ddict.decompress(allocator, compressed);
```

### Parameter Bounds

```zig
const window_log = try zstd.cParamGetBounds(.window_log);
std.debug.print("Window log: {d} to {d}\n", .{ window_log.lower_bound, window_log.upper_bound });

const hash_log = try zstd.cParamGetBounds(.hash_log);
std.debug.print("Hash log: {d} to {d}\n", .{ hash_log.lower_bound, hash_log.upper_bound });
```

## API Reference

### Top-Level Functions

| Function | Description |
|---|---|
| `zstd.compress(alloc, src, opts)` | One-shot compression |
| `zstd.decompress(alloc, src, opts)` | One-shot decompression |
| `zstd.compressBound(src_size)` | Maximum compressed size for buffer allocation |
| `zstd.trainFromSamples(alloc, buf, sizes, cap)` | Train dictionary from samples |
| `zstd.finalizeDictionary(dst, max, content, samples, sizes, params)` | Create dictionary from content |
| `zstd.compressUsingDict(alloc, src, dict, level)` | Compress with raw dictionary data |
| `zstd.decompressUsingDict(alloc, src, dict)` | Decompress with raw dictionary data |
| `zstd.cParamGetBounds(param)` | Query compression parameter bounds |
| `zstd.dParamGetBounds(param)` | Query decompression parameter bounds |

### Types

| Type | Description |
|---|---|
| `Compressor` | Reusable compression context with `compress2()` |
| `Decompressor` | Reusable decompression context with configurable limits |
| `StreamCompressor` | Streaming compression with `compressChunk()` / `endStream()` |
| `StreamDecompressor` | Streaming decompression with `decompressChunk()` |
| `CDict` | Prepared compression dictionary with `compress()` method |
| `DDict` | Prepared decompression dictionary with `decompress()` method |
| `CLevel` | Compression level enum (`.fastest`, `.default`, `.best`, `.raw`) |
| `CompressOptions` | Compression options (level, checksum, dict_id, strategy, window_log) |
| `DecompressOptions` | Decompression options (max_window_size, max_output_size, dict) |
| `StreamCompressOptions` | Streaming compression options (level, checksum) |
| `StreamDecompressOptions` | Streaming decompression options (dict) |
| `CParameter` | Compression parameter enum (window_log, hash_log, chain_log, etc.) |
| `DParameter` | Decompression parameter enum (window_log_max) |
| `Strategy` | Compression strategy enum (fast, greedy, lazy, btopt, btultra, etc.) |
| `Bounds` | Parameter bounds (lower_bound, upper_bound) |
| `FrameInfo` | Frame metadata (content_size, window_size, dictionary_id, checksum) |

### Namespaces

| Namespace | Description |
|---|---|
| `zstd.Frame` | Frame inspection: `isFrame()`, `inspect()`, `contentSize()`, `compressedSize()`, `dictId()` |
| `zstd.version` | Version info: `number`, `string`, `major`, `minor`, `release`, `clevel_default`, `clevel_min`, `clevel_max` |
| `zstd.constants` | Constants: `magic_number`, `magic_dictionary`, `block_size_max`, `max_input_size` |

### Convenience Aliases

```zig
// Compression
const compressed = try zstd.compress(alloc, data, .{});
const decompressed = try zstd.decompress(alloc, compressed, .{});

// Frame detection
const is_valid = zstd.isFrame(data);
const content = zstd.getFrameContentSize(data);
const size = try zstd.findFrameCompressedSize(data);

// Version
const ver = zstd.versionNumber();
const str = zstd.versionString();
const min = zstd.minCLevel();
const max = zstd.maxCLevel();
const def = zstd.defaultCLevel();
```

## Examples

The `examples/` directory contains **9 comprehensive, runnable examples** demonstrating all features:

| Example | Description |
|---------|-------------|
| [`basic`](examples/basic.zig) | Basic compress/decompress round trip |
| [`streaming`](examples/streaming.zig) | Streaming compression with chunked input |
| [`decompress`](examples/decompress.zig) | Decompression with pattern verification |
| [`compression-levels`](examples/compression-levels.zig) | Named and numeric compression levels |
| [`dictionary`](examples/dictionary.zig) | Dictionary creation, CDict compress, DDict decompress |
| [`frame-inspection`](examples/frame-inspection.zig) | Frame metadata extraction and hex dump |
| [`advanced-parameters`](examples/advanced-parameters.zig) | Parameter bounds queries and strategy comparison |
| [`custom-allocator`](examples/custom-allocator.zig) | Page allocator and arena allocator usage |
| [`streaming-decompress`](examples/streaming-decompress.zig) | Streaming decompression with multiple round trips |

To run any example:

```bash
zig build run-basic
zig build run-streaming
zig build run-decompress
zig build run-compression-levels
zig build run-dictionary
zig build run-frame-inspection
zig build run-advanced-parameters
zig build run-custom-allocator
zig build run-streaming-decompress
```

## Building & Testing

```bash
zig build            # Build library
zig build test       # Run all tests (35+)
zig build fmt        # Format source files
zig build docs       # Generate documentation site
```

### Run All Examples

```bash
zig build run-basic
zig build run-streaming
zig build run-decompress
zig build run-compression-levels
zig build run-dictionary
zig build run-frame-inspection
zig build run-advanced-parameters
zig build run-custom-allocator
zig build run-streaming-decompress
```

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass: `zig build test`
5. Ensure formatting passes: `zig build fmt`
6. Submit a pull request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

## License

MIT License - see [LICENSE](LICENSE) for details.

## Author

**Muhammad Fiaz** (https://github.com/muhammad-fiaz)
