//! # zstd.zig - High-Level Zig Bindings for Zstandard
//!
//! Complete, idiomatic, high-level native Zig bindings for the zstd (Zstandard)
//! compression library v1.6.0. This library wraps the full Facebook/Meta zstd
//! C API surface through safe, well-documented Zig APIs.
//!
//! Requires Zig 0.16.0 or later.
//!
//! ## Quick Start
//!
//! ```zig
//! const zstd = @import("zstd");
//!
//! fn compressExample(allocator: std.mem.Allocator) !void {
//!     const data = "Hello, zstd from Zig!";
//!     const compressed = try zstd.compress(allocator, data, 3);
//!     defer allocator.free(compressed);
//!
//!     const decompressed = try zstd.decompress(allocator, compressed, data.len);
//!     defer allocator.free(decompressed);
//!
//!     std.debug.print("Compressed {d} -> {d} bytes\n", .{ data.len, compressed.len });
//! }
//! ```
//!
//! ## API Modules
//!
//! - `errors` - Unified Zig error set for all zstd error codes
//! - `version` - Library version and capability queries
//! - `simple` - One-shot compress/decompress functions
//! - `cctx` - Explicit compression context (`Compressor`) with parameter tuning
//! - `dctx` - Explicit decompression context (`Decompressor`) with parameter tuning
//! - `stream` - Streaming compression/decompression for large data
//! - `dict` - Dictionary compression/decompression with `CDict` and `DDict`
//! - `zdict` - Dictionary builder for training dictionaries from sample data
//!
//! ## Building
//!
//! Add this package as a dependency in your `build.zig.zon`, then import it
//! in your `build.zig`:
//!
//! ```zig
//! const zstd_dep = b.dependency("zstd", .{});
//! exe.root_module.addImport("zstd", zstd_dep.module("zstd"));
//! ```

const std = @import("std");

/// Error handling and error code mapping.
pub const errors = @import("errors.zig");

/// Library version and capability information.
pub const version = @import("version.zig");

/// One-shot compression and decompression API.
pub const simple = @import("simple.zig");

/// Explicit compression context API.
pub const cctx = @import("cctx.zig");

/// Explicit decompression context API.
pub const dctx = @import("dctx.zig");

/// Streaming compression and decompression API.
pub const stream = @import("stream.zig");

/// Dictionary compression and decompression API.
pub const dict = @import("dict.zig");

/// Dictionary builder API (ZDICT).
pub const zdict = @import("zdict.zig");

// Re-export key types at top level for convenience
pub const ZstdError = errors.ZstdError;
pub const Compressor = cctx.Compressor;
pub const Decompressor = dctx.Decompressor;
pub const CDict = cctx.CDict;
pub const DDict = dctx.DDict;
pub const CParameter = cctx.CParameter;
pub const DParameter = dctx.DParameter;
pub const Strategy = cctx.Strategy;
pub const ResetDirective = cctx.ResetDirective;
pub const EndDirective = stream.EndDirective;
pub const StreamingCompressor = stream.StreamingCompressor;
pub const StreamingDecompressor = stream.StreamingDecompressor;
pub const ContentSizeResult = simple.ContentSizeResult;
pub const DictParams = zdict.DictParams;

// Re-export convenience functions at top level
pub const compress = simple.compress;
pub const decompress = simple.decompress;
pub const compressBound = simple.compressBound;
pub const getFrameContentSize = simple.getFrameContentSize;
pub const findFrameCompressedSize = simple.findFrameCompressedSize;
pub const isFrame = simple.isFrame;
pub const versionNumber = version.versionNumber;
pub const versionString = version.versionString;
pub const minCLevel = version.minCLevel;
pub const maxCLevel = version.maxCLevel;
pub const defaultCLevel = version.defaultCLevel;

test {
    _ = errors;
    _ = version;
    _ = simple;
    _ = cctx;
    _ = dctx;
    _ = stream;
    _ = dict;
    _ = zdict;
}
