//! Dictionary API for zstd.
//!
//! Wraps `ZSTD_compress_usingDict`, `ZSTD_decompress_usingDict`,
//! `ZSTD_compress_usingCDict`, `ZSTD_decompress_usingDDict`,
//! `ZSTD_CDict`, `ZSTD_DDict`, `ZSTD_getDictID_fromDict`,
//! `ZSTD_getDictID_fromCDict`, `ZSTD_getDictID_fromDDict`,
//! `ZSTD_getDictID_fromFrame`, `ZSTD_sizeof_CDict`, and
//! `ZSTD_sizeof_DDict` from the Dictionary API section of `zstd.h`.

const std = @import("std");
const errors = @import("errors.zig");
const ZstdError = errors.ZstdError;

pub const cctx = @import("cctx.zig");
pub const dctx = @import("dctx.zig");

const c = @cImport({
    @cInclude("zstd.h");
});

/// Compresses `src` using `dict` as a dictionary in a single operation.
///
/// This wraps `ZSTD_compress_usingDict`. The dictionary is loaded fresh each
/// time, so this is intended for one-shot dictionary usage. For repeated use,
/// prefer `CDict` and `compressUsingCDict`.
///
/// `level` is the compression level. Pass `null` for the dictionary to compress
/// without a dictionary.
pub fn compressUsingDict(
    allocator: std.mem.Allocator,
    src: []const u8,
    dict: ?[]const u8,
    level: i32,
) ZstdError![]u8 {
    const ctx = c.ZSTD_createCCtx() orelse return error.MemoryAllocation;
    defer _ = c.ZSTD_freeCCtx(ctx);

    const d = dict orelse &[_]u8{};
    const bound = try errors.check(c.ZSTD_compressBound(src.len));
    const dst = try allocator.alloc(u8, bound);
    errdefer allocator.free(dst);

    const written = errors.check(c.ZSTD_compress_usingDict(
        ctx,
        dst.ptr,
        dst.len,
        src.ptr,
        src.len,
        d.ptr,
        d.len,
        @intCast(level),
    )) catch |err| {
        allocator.free(dst);
        return err;
    };
    return allocator.realloc(dst, written) catch dst;
}

/// Decompresses `src` using `dict` as a dictionary in a single operation.
///
/// This wraps `ZSTD_decompress_usingDict`. The dictionary must be identical
/// to the one used during compression. For repeated use, prefer `DDict` and
/// `decompressUsingDDict`.
///
/// Pass `null` for the dictionary to decompress without a dictionary.
pub fn decompressUsingDict(
    allocator: std.mem.Allocator,
    src: []const u8,
    dict: ?[]const u8,
    expected_size: usize,
) ZstdError![]u8 {
    const ctx = c.ZSTD_createDCtx() orelse return error.MemoryAllocation;
    defer _ = c.ZSTD_freeDCtx(ctx);

    const d = dict orelse &[_]u8{};
    const dst = try allocator.alloc(u8, expected_size);
    errdefer allocator.free(dst);

    const written = errors.check(c.ZSTD_decompress_usingDict(
        ctx,
        dst.ptr,
        dst.len,
        src.ptr,
        src.len,
        d.ptr,
        d.len,
    )) catch |err| {
        allocator.free(dst);
        return err;
    };
    return allocator.realloc(dst, written) catch dst;
}

/// Compresses `src` using a prepared compression dictionary.
///
/// This wraps `ZSTD_compress_usingCDict`. Recommended when the same dictionary
/// is used multiple times. The compression level is decided at dictionary
/// creation time.
pub fn compressUsingCDict(
    allocator: std.mem.Allocator,
    src: []const u8,
    cdict: *const cctx.CDict,
) ZstdError![]u8 {
    const ctx = c.ZSTD_createCCtx() orelse return error.MemoryAllocation;
    defer _ = c.ZSTD_freeCCtx(ctx);

    const bound = try errors.check(c.ZSTD_compressBound(src.len));
    const dst = try allocator.alloc(u8, bound);
    errdefer allocator.free(dst);

    const written = errors.check(c.ZSTD_compress_usingCDict(
        ctx,
        dst.ptr,
        dst.len,
        src.ptr,
        src.len,
        @ptrCast(cdict.ptr),
    )) catch |err| {
        allocator.free(dst);
        return err;
    };
    return allocator.realloc(dst, written) catch dst;
}

/// Decompresses `src` using a prepared decompression dictionary.
///
/// This wraps `ZSTD_decompress_usingDDict`. Recommended when the same dictionary
/// is used multiple times.
pub fn decompressUsingDDict(
    allocator: std.mem.Allocator,
    src: []const u8,
    ddict: *const dctx.DDict,
    expected_size: usize,
) ZstdError![]u8 {
    const dctx_inst = try dctx.Decompressor.init();
    defer dctx_inst.deinit();

    _ = try errors.check(c.ZSTD_DCtx_refDDict(@ptrCast(dctx_inst.ctx), @ptrCast(ddict.ptr)));

    const dst = try allocator.alloc(u8, expected_size);
    errdefer allocator.free(dst);

    const written = errors.check(c.ZSTD_decompress_usingDDict(
        @ptrCast(dctx_inst.ctx),
        dst.ptr,
        dst.len,
        src.ptr,
        src.len,
        @ptrCast(ddict.ptr),
    )) catch |err| {
        allocator.free(dst);
        return err;
    };
    return allocator.realloc(dst, written) catch dst;
}

/// Returns the dictionary ID stored within a raw dictionary buffer.
///
/// This wraps `ZSTD_getDictID_fromDict`. Returns 0 if the dictionary is
/// not conformant with the zstd specification.
pub fn getDictIDFromDict(dict: []const u8) u32 {
    return @intCast(c.ZSTD_getDictID_fromDict(dict.ptr, dict.len));
}

/// Returns the dictionary ID stored within a frame.
///
/// This wraps `ZSTD_getDictID_fromFrame`. Returns 0 if the dictID could not
/// be decoded (e.g. no dictionary required, or frame too small).
pub fn getDictIDFromFrame(src: []const u8) u32 {
    return @intCast(c.ZSTD_getDictID_fromFrame(src.ptr, src.len));
}

test "compressUsingDict round trip" {
    const original = "Dictionary compression test";
    const dict_content = "dictionary content for testing purposes that should help with compression of small data";
    const compressed = try compressUsingDict(std.testing.allocator, original, dict_content, 3);
    defer std.testing.allocator.free(compressed);

    const decompressed = try decompressUsingDict(std.testing.allocator, compressed, dict_content, original.len + 64);
    defer std.testing.allocator.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
}

test "CDict compress round trip" {
    const original = "CDict round trip test";
    const dict_content = "reusable dictionary content for CDict testing that is sufficiently long to be useful";

    var cdict = try cctx.CDict.init(dict_content, 3);
    defer cdict.deinit();

    const compressed = try compressUsingCDict(std.testing.allocator, original, &cdict);
    defer std.testing.allocator.free(compressed);

    var ddict = try dctx.DDict.init(dict_content);
    defer ddict.deinit();

    const decompressed = try decompressUsingDDict(std.testing.allocator, compressed, &ddict, original.len + 64);
    defer std.testing.allocator.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
}
