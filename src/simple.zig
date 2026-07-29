//! One-shot compression and decompression API.
//!
//! Wraps `ZSTD_compress`, `ZSTD_decompress`, `ZSTD_compressBound`,
//! `ZSTD_getFrameContentSize`, `ZSTD_findFrameCompressedSize`, and
//! `ZSTD_isFrame` from the Simple Core API section of `zstd.h`.

const std = @import("std");
const errors = @import("errors.zig");

const ZstdError = errors.ZstdError;

const c = @cImport({
    @cInclude("zstd.h");
});

/// Maximum compressed size in worst-case single-pass scenario.
///
/// When invoking `compress`, providing a destination buffer of at least
/// `compressBound(src.len)` bytes guarantees success (barring other errors).
///
/// This wraps `ZSTD_compressBound`. Returns `error.SrcSizeWrong` if
/// `src_size >= ZSTD_MAX_INPUT_SIZE`.
pub fn compressBound(src_size: usize) ZstdError!usize {
    return errors.check(c.ZSTD_compressBound(src_size));
}

/// Compresses `src` into a newly allocated buffer using the default one-shot API.
///
/// This wraps `ZSTD_compress`. The returned buffer is allocated with `allocator`
/// and owned by the caller; free it with `allocator.free(result)`.
///
/// `level` must be between `minCLevel()` and `maxCLevel()` inclusive. Values
/// above `ZSTD_CLEVEL_DEFAULT` trade speed for a better compression ratio.
///
/// Returns `error.DstSizeTooSmall`, `error.GenericError`, or another member of
/// `ZstdError` if the underlying C call fails; see `errors.zig` for the full
/// error-to-`ZSTD_ErrorCode` mapping.
pub fn compress(allocator: std.mem.Allocator, src: []const u8, level: i32) ZstdError![]u8 {
    const bound = try compressBound(src.len);
    const dst = try allocator.alloc(u8, bound);
    errdefer allocator.free(dst);

    const written = errors.check(c.ZSTD_compress(
        dst.ptr,
        dst.len,
        src.ptr,
        src.len,
        @intCast(level),
    )) catch |err| {
        allocator.free(dst);
        return err;
    };
    return allocator.realloc(dst, written) catch dst;
}

/// Decompresses a zstd-compressed frame into a newly allocated buffer.
///
/// This wraps `ZSTD_decompress`. `expected_size_hint` is the upper bound for
/// the decompressed size. If you know the exact size from the frame header
/// (via `getFrameContentSize`), pass that. Otherwise, use a generous upper bound
/// or prefer streaming decompression.
///
/// The returned buffer is allocated with `allocator` and owned by the caller.
pub fn decompress(allocator: std.mem.Allocator, src: []const u8, expected_size_hint: usize) ZstdError![]u8 {
    const dst = try allocator.alloc(u8, expected_size_hint);
    errdefer allocator.free(dst);

    const written = errors.check(c.ZSTD_decompress(
        dst.ptr,
        dst.len,
        src.ptr,
        src.len,
    )) catch |err| {
        allocator.free(dst);
        return err;
    };
    return allocator.realloc(dst, written) catch dst;
}

/// Result of inspecting a frame's content size.
pub const ContentSizeResult = union(enum) {
    /// The decompressed size is known and stored in the frame header.
    known: u64,
    /// The frame does not encode a decompressed size (typical for streaming).
    unknown,
    /// An error occurred (e.g. invalid magic number, source too small).
    @"error",
};

/// Returns the decompressed content size stored in a zstd frame header.
///
/// This wraps `ZSTD_getFrameContentSize`. A return value of `0` denotes a
/// valid but empty frame. Skippable frames also report 0.
///
/// The decompressed size is guaranteed to be present when compression was
/// performed with single-pass APIs such as `compress`, `Compressor.compress2`,
/// or `compressUsingDict`.
pub fn getFrameContentSize(src: []const u8) ContentSizeResult {
    const result = c.ZSTD_getFrameContentSize(src.ptr, src.len);
    if (result == ~@as(c_ulonglong, 0)) return .unknown;
    if (result == ~@as(c_ulonglong, 0) - 1) return .@"error";
    return .{ .known = result };
}

/// Returns the compressed size of the first frame starting at `src`.
///
/// This wraps `ZSTD_findFrameCompressedSize`. The `src` buffer must be large
/// enough to contain at least one complete frame.
pub fn findFrameCompressedSize(src: []const u8) ZstdError!usize {
    return errors.check(c.ZSTD_findFrameCompressedSize(src.ptr, src.len));
}

/// Returns `true` if `src` begins with a valid zstd frame magic number.
///
/// This is a quick check; it does not validate the entire frame.
pub fn isFrame(src: []const u8) bool {
    if (src.len < 4) return false;
    const magic: u32 = @as(u32, src[0]) |
        (@as(u32, src[1]) << 8) |
        (@as(u32, src[2]) << 16) |
        (@as(u32, src[3]) << 24);
    return magic == 0xFD2FB528;
}

/// Decompresses a zstd-compressed frame into a newly allocated buffer.
///
/// This wraps `ZSTD_findDecompressedSize`. It is superseded by
/// `getFrameContentSize` which distinguishes unknown and error cases.
/// The decompressed size is guaranteed to be present when compression was
/// performed with single-pass APIs.
pub fn findDecompressedSize(src: []const u8) ZstdError!u64 {
    const result = errors.check(c.ZSTD_findDecompressedSize(src.ptr, src.len)) catch |err| return err;
    return @intCast(result);
}

test "compress decompress round trip" {
    const original = "Hello, zstd from Zig!";
    const compressed = try compress(std.testing.allocator, original, 3);
    defer std.testing.allocator.free(compressed);

    const decompressed = try decompress(std.testing.allocator, compressed, original.len);
    defer std.testing.allocator.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
}

test "compressBound works" {
    const bound = try compressBound(1024);
    try std.testing.expect(bound > 1024);
}

test "getFrameContentSize round trip" {
    const original = "Test content size";
    const compressed = try compress(std.testing.allocator, original, 1);
    defer std.testing.allocator.free(compressed);

    const cs = getFrameContentSize(compressed);
    try std.testing.expect(cs.known == original.len);
}

test "isFrame detects valid frames" {
    const original = "frame magic";
    const compressed = try compress(std.testing.allocator, original, 1);
    defer std.testing.allocator.free(compressed);

    try std.testing.expect(isFrame(compressed));
    try std.testing.expect(!isFrame("not a zstd frame"));
    try std.testing.expect(!isFrame(&[_]u8{ 0x01, 0x02 }));
}
