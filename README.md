# zstd.zig

Zig bindings for Facebook's [zstd](https://github.com/facebook/zstd) (Zstandard) fast compression library. Wraps the full zstd C API (`zstd.h`, `zstd_errors.h`, `zdict.h`) into idiomatic, safe, well-documented Zig modules.

> [!INFO]
> * **Binding version:** 0.0.1
> * **Zig version:** 0.16.0 or later
> * **zstd C library version:** 1.6.0 (upstream commit [`5c7b7bad`](https://github.com/facebook/zstd/commit/5c7b7bad26808e6b40ac3b3d0075466e27738a9d))

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

## Requirements

> [!INFO]
> * Zig 0.16.0 or later is required.

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

## API Reference

All functions are available directly from `zstd`:

```zig
const zstd = @import("zstd");
```

### Single-shot Compression

Compress and decompress data in one call. Best for data that fits in memory.

| Function | Signature | Description |
|---|---|---|
| `zstd.compress` | `(allocator, src: []const u8, level: i32) ZstdError![]u8` | Compress data. Returns allocated buffer. |
| `zstd.decompress` | `(allocator, src: []const u8, expected_size: usize) ZstdError![]u8` | Decompress data. Returns allocated buffer. |
| `zstd.compressBound` | `(src_size: usize) ZstdError!usize` | Worst-case compressed size for allocation. |
| `zstd.getFrameContentSize` | `(src: []const u8) ContentSizeResult` | Decompressed size from frame header. |
| `zstd.findFrameCompressedSize` | `(src: []const u8) ZstdError!usize` | Compressed size of first frame in buffer. |
| `zstd.isFrame` | `(src: []const u8) bool` | Check for valid zstd frame magic number. |
| `zstd.findDecompressedSize` | `(src: []const u8) ZstdError!u64` | Total decompressed size (deprecated, use getFrameContentSize). |

**ContentSizeResult** is a tagged union:
* `.known: u64` - size is known
* `.unknown` - size not stored in frame header
* `.@"error"` - invalid frame or source too small

**Example:**

```zig
const compressed = try zstd.compress(allocator, "hello world", 3);
defer allocator.free(compressed);

// Check frame info
const size = zstd.getFrameContentSize(compressed);
switch (size) {
    .known => |s| std.debug.print("Decompressed size: {d}\n", .{s}),
    .unknown => std.debug.print("Size unknown\n", .{}),
    .@"error" => unreachable,
}
```

### Streaming Compression and Decompression

Process data in chunks. Use for data too large to fit in memory or for real-time compression.

#### StreamingCompressor

| Method | Signature | Description |
|---|---|---|
| `init` | `() !StreamingCompressor` | Create a new streaming compressor. |
| `deinit` | `(self) void` | Free the compressor. |
| `initStream` | `(self, level: i32) ZstdError!void` | Initialize for a new compression run. |
| `reset` | `(self) ZstdError!void` | Reset session state. |
| `setParameter` | `(self, param: CParameter, value: i32) ZstdError!void` | Set a compression parameter. |
| `compressChunk` | `(self, input: []const u8, output: []u8, end_op: EndDirective) ZstdError!StreamResult` | Compress a chunk of data. |
| `endStream` | `(self, output: []u8) ZstdError!StreamResult` | Flush and close the frame. |
| `flushStream` | `(self, output: []u8) ZstdError!StreamResult` | Flush without closing. |

#### StreamingDecompressor

| Method | Signature | Description |
|---|---|---|
| `init` | `() !StreamingDecompressor` | Create a new streaming decompressor. |
| `deinit` | `(self) void` | Free the decompressor. |
| `initStream` | `(self) ZstdError!void` | Initialize for a new decompression run. |
| `decompressChunk` | `(self, input: []const u8, output: []u8) ZstdError!StreamResult` | Decompress a chunk of data. |

#### StreamResult

```zig
pub const StreamResult = struct {
    bytes_written: usize,  // Bytes written to output buffer
    remaining: usize,      // Bytes still in internal buffers (0 = done)
};
```

#### EndDirective

```zig
pub const EndDirective = enum(c_int) {
    @"continue" = 0,  // Collect more data
    flush = 1,        // Flush what you have
    end = 2,          // Flush and close frame
};
```

**Buffer size recommendations:**

```zig
const in_buf_size = zstd.stream.cStreamInSize();   // ~128 KB
const out_buf_size = zstd.stream.cStreamOutSize();  // ~128 KB
const dstream_out = zstd.stream.dStreamOutSize();   // Recommended decompress buffer
```

**Example:**

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

### Compression Context (CCtx)

Full control over compression parameters. Reuse across multiple operations.

#### Compressor

| Method | Signature | Description |
|---|---|---|
| `init` | `() !Compressor` | Create a new compression context. |
| `deinit` | `(self) void` | Free the context. |
| `setParameter` | `(self, param: CParameter, value: i32) ZstdError!void` | Set a compression parameter. |
| `setPledgedSrcSize` | `(self, src_size: u64) ZstdError!void` | Set input size for frame header. |
| `reset` | `(self, directive: ResetDirective) ZstdError!void` | Reset session/parameters/both. |
| `compress2` | `(self, dst: []u8, src: []const u8) ZstdError!usize` | Compress into pre-allocated buffer. |
| `compressAlloc` | `(self, allocator, src: []const u8) ZstdError![]u8` | Compress into newly allocated buffer. |
| `loadDictionary` | `(self, dict: ?[]const u8) ZstdError!void` | Load a raw dictionary. |
| `refCDict` | `(self, cdict: ?*const CDict) ZstdError!void` | Reference a prepared dictionary. |
| `refPrefix` | `(self, prefix: ?[]const u8) ZstdError!void` | Reference a single-use prefix dictionary. |
| `sizeof` | `(self) usize` | Current memory usage in bytes. |

#### CParameter

| Variant | Value | Description |
|---|---|---|
| `compression_level` | 100 | Compression level (1 to 22, default 3) |
| `window_log` | 101 | Maximum back-reference distance (log2) |
| `hash_log` | 102 | Hash table size (log2) |
| `chain_log` | 103 | Chain table size (log2, greedy+) |
| `search_log` | 104 | Search depth (log2, lazy+) |
| `min_match` | 105 | Minimum match length |
| `target_length` | 106 | Target match length |
| `strategy` | 107 | Compression strategy (see Strategy enum) |
| `content_size_flag` | 200 | Write content size in frame header |
| `checksum_flag` | 201 | Add XXH64 checksum to frame |
| `dict_id_flag` | 202 | Write dictionary ID in frame |
| `nb_workers` | 400 | Number of compression threads (0 = single-threaded) |
| `job_size` | 401 | Size of each compression job |
| `overlap_log` | 402 | Overlap between jobs |

#### Strategy

From fastest to strongest:

```zig
pub const Strategy = enum(c_int) {
    fast = 1,      // Fastest, lowest ratio
    dfast = 2,     // Double-fast
    greedy = 3,    // Greedy
    lazy = 4,      // Lazy
    lazy2 = 5,     // Lazy2
    btlazy2 = 6,   // BT lazy2
    btopt = 7,     // BT optimal
    btultra = 8,   // BT ultra
    btultra2 = 9,  // BT ultra2 (strongest)
};
```

#### ResetDirective

```zig
pub const ResetDirective = enum(c_int) {
    session_only = 1,           // Start new frame, keep parameters
    parameters = 2,             // Reset parameters, keep session
    session_and_parameters = 3, // Reset both
};
```

**Example:**

```zig
var comp = try zstd.Compressor.init();
defer comp.deinit();

try comp.setParameter(.compression_level, 10);
try comp.setParameter(.nb_workers, 4);
try comp.setParameter(.checksum_flag, 1);

const compressed = try comp.compressAlloc(allocator, data);
defer allocator.free(compressed);
```

#### cParamGetBounds

Query valid range for a compression parameter:

```zig
const bounds = try zstd.cctx.cParamGetBounds(.compression_level);
std.debug.print("Level range: [{d}, {d}]\n", .{ bounds.lower_bound, bounds.upper_bound });
```

### Decompression Context (DCtx)

Full control over decompression parameters.

#### Decompressor

| Method | Signature | Description |
|---|---|---|
| `init` | `() !Decompressor` | Create a new decompression context. |
| `deinit` | `(self) void` | Free the context. |
| `setParameter` | `(self, param: DParameter, value: i32) ZstdError!void` | Set a decompression parameter. |
| `reset` | `(self, directive: ResetDirective) ZstdError!void` | Reset session/parameters/both. |
| `decompress` | `(self, dst: []u8, src: []const u8) ZstdError!usize` | Decompress into pre-allocated buffer. |
| `decompressAlloc` | `(self, allocator, src: []const u8, expected: usize) ZstdError![]u8` | Decompress into newly allocated buffer. |
| `loadDDictionary` | `(self, dict: ?[]const u8) ZstdError!void` | Load a raw dictionary. |
| `refDDict` | `(self, ddict: ?*const DDict) ZstdError!void` | Reference a prepared dictionary. |
| `refPrefix` | `(self, prefix: ?[]const u8) ZstdError!void` | Reference a single-use prefix dictionary. |
| `sizeof` | `(self) usize` | Current memory usage in bytes. |

#### DParameter

| Variant | Value | Description |
|---|---|---|
| `window_log_max` | 100 | Maximum window size for decompression |

**Example:**

```zig
var decomp = try zstd.Decompressor.init();
defer decomp.deinit();

const decompressed = try decomp.decompressAlloc(allocator, compressed, 1024);
defer allocator.free(decompressed);
```

### Dictionary Compression

Reuse learned dictionaries for better compression on similar data.

#### Function-based (one-shot)

| Function | Signature | Description |
|---|---|---|
| `zstd.zdict.compressUsingDict` | `(allocator, src, dict, level) ZstdError![]u8` | Compress with a raw dictionary. |
| `zstd.zdict.decompressUsingDict` | `(allocator, src, ddict, expected) ZstdError![]u8` | Decompress with a raw dictionary. |
| `zstd.zdict.compressUsingCDict` | `(allocator, src, cdict, level) ZstdError![]u8` | Compress with a prepared CDict. |
| `zstd.zdict.decompressUsingDDict` | `(allocator, src, ddict, expected) ZstdError![]u8` | Decompress with a prepared DDict. |

#### Dictionary Types

**CDict** (prepared compression dictionary):

| Method | Description |
|---|---|
| `CDict.init(dict_buffer, level)` | Create from raw dictionary bytes. |
| `CDict.deinit()` | Free the dictionary. |
| `CDict.getDictId()` | Get the dictionary ID. |
| `CDict.sizeof()` | Memory usage in bytes. |

**DDict** (prepared decompression dictionary):

| Method | Description |
|---|---|
| `DDict.init(dict_buffer)` | Create from raw dictionary bytes. |
| `DDict.deinit()` | Free the dictionary. |
| `DDict.getDictId()` | Get the dictionary ID. |
| `DDict.sizeof()` | Memory usage in bytes. |

**Example:**

```zig
// Create dictionaries from raw dictionary bytes
var cdict = try zstd.CDict.init(dict_bytes, 3);
defer cdict.deinit();

var ddict = try zstd.DDict.init(dict_bytes);
defer ddict.deinit();

// Compress and decompress with prepared dictionaries
const compressed = try zstd.zdict.compressUsingCDict(allocator, data, &cdict, 3);
defer allocator.free(compressed);

const decompressed = try zstd.zdict.decompressUsingDDict(allocator, compressed, &ddict, data.len);
defer allocator.free(decompressed);
```

### Dictionary Builder (ZDICT)

Train compression dictionaries from sample data.

| Function | Signature | Description |
|---|---|---|
| `zstd.zdict.trainFromSamples` | `(allocator, samples, dict_capacity, params) ZstdError![]u8` | Train dictionary from samples. |
| `zstd.zdict.finalizeDictionary` | `(allocator, dict_content, samples, params) ZstdError![]u8` | Finalize a custom dictionary. |
| `zstd.zdict.getDictID` | `(dict: []const u8) u32` | Get dictionary ID from raw dict. |
| `zstd.zdict.getDictHeaderSize` | `(dict: []const u8) usize` | Get header size of a dictionary. |
| `zstd.zdict.isError` | `(result: usize) bool` | Check if a ZDICT result is an error. |
| `zstd.zdict.dictErrorName` | `(result: usize) [*:0]const u8` | Get error name string. |

#### DictParams

```zig
pub const DictParams = struct {
    compression_level: i32 = 0,
    notification_level: u32 = 0,
    dict_id: u32 = 0,
};
```

#### CoverParams

```zig
pub const CoverParams = struct {
    k: u32 = 0,           // Number of patterns
    d: u32 = 0,           // Derivative level
    steps: u32 = 0,       // Number of steps
    nb_threads: u32 = 0,  // Number of threads
    split_point: f64 = 0, // Split point for training
    // ...
};
```

**Example:**

```zig
// Prepare sample data
const samples = [_][]const u8{
    "sample one for training",
    "sample two for training",
    "sample three for training",
};

// Train a 16 KB dictionary
const dict = try zstd.zdict.trainFromSamples(
    allocator,
    &samples,
    1 << 14, // 16 KB
    .{},
);
defer allocator.free(dict);

std.debug.print("Trained dictionary: {d} bytes\n", .{dict.len});
```

### Error Handling

All zstd errors are mapped to the Zig `ZstdError` error set:

| Error | zstd Error Code | Description |
|---|---|---|
| `OutOfMemory` | N/A | Zig allocator failure |
| `GenericError` | 1 | Generic error |
| `PrefixUnknown` | 10 | Unknown frame prefix |
| `VersionUnsupported` | 12 | Unsupported version |
| `FrameParameterUnsupported` | 14 | Unsupported frame parameter |
| `FrameParameterWindowTooLarge` | 16 | Window size too large |
| `CorruptionDetected` | 20 | Data corruption detected |
| `ChecksumWrong` | 22 | Frame checksum mismatch |
| `LiteralsHeaderWrong` | 24 | Invalid literals header |
| `DictionaryCorrupted` | 30 | Dictionary corrupted |
| `DictionaryWrong` | 32 | Dictionary incompatible |
| `DictionaryCreationFailed` | 34 | Failed to create dictionary |
| `ParameterUnsupported` | 40 | Unsupported parameter |
| `ParameterCombinationUnsupported` | 41 | Invalid parameter combination |
| `ParameterOutOfBound` | 42 | Parameter out of valid range |
| `MemoryAllocation` | 64 | C library memory allocation failed |
| `DstSizeTooSmall` | 70 | Destination buffer too small |
| `SrcSizeWrong` | 72 | Invalid source size |
| `DstBufferNull` | 74 | Destination buffer is null |

**Example:**

```zig
const compressed = zstd.compress(allocator, data, 3) catch |err| switch (err) {
    error.DstSizeTooSmall => {
        std.debug.print("Buffer too small\n", .{});
        return;
    },
    error.OutOfMemory => return error.OutOfMemory,
    else => {
        std.debug.print("Compression error: {s}\n", .{@errorName(err)});
        return;
    },
};
```

### Version Queries

| Function | Signature | Description |
|---|---|---|
| `zstd.versionNumber` | `() u32` | Version as `MAJOR*10000 + MINOR*100 + RELEASE` |
| `zstd.versionString` | `() [*:0]const u8` | Version string, e.g. `"1.6.0"` |
| `zstd.minCLevel` | `() i32` | Minimum compression level (negative, ultra-fast) |
| `zstd.maxCLevel` | `() i32` | Maximum compression level (22) |
| `zstd.defaultCLevel` | `() i32` | Default compression level (3) |

**Example:**

```zig
std.debug.print("zstd version: {s}\n", .{zstd.versionString()});
std.debug.print("Level range: [{d}, {d}], default: {d}\n", .{
    zstd.minCLevel(),
    zstd.maxCLevel(),
    zstd.defaultCLevel(),
});
```

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
zig build example-error-handling
zig build example-cctx-parameters
zig build example-frame-content-size
```

## Platform Support

> [!INFO]
> Supported on Windows, macOS, and Linux. Works on x86_64, aarch64, and other targets supported by Zig and zstd.

## Authors

> [!INFO]
> * **Muhammad Fiaz** (https://github.com/muhammad-fiaz) - Zig bindings, build system, and API design
> * **Meta Platforms (Facebook)** (https://github.com/facebook/zstd) - Original zstd C library

## License

BSD + GPLv2 (see [LICENSE](LICENSE) and [COPYING](COPYING)).

Original zstd code: Copyright (c) Meta Platforms, Inc. and affiliates.

Zig bindings: Copyright (c) Muhammad Fiaz.
