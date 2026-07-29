//! Explicit decompression context API.
//!
//! Wraps `ZSTD_DCtx`, `ZSTD_DCtx_setParameter`, `ZSTD_DCtx_reset`,
//! `ZSTD_DCtx_loadDictionary`, `ZSTD_DCtx_refDDict`, `ZSTD_DCtx_refPrefix`,
//! and the `ZSTD_dParameter` enum from the Advanced Decompression API section
//! of `zstd.h`.

const std = @import("std");
const errors = @import("errors.zig");
const ZstdError = errors.ZstdError;

const c = @cImport({
    @cInclude("zstd.h");
});

/// Decompression parameters that can be set on a `Decompressor`.
pub const DParameter = enum(c_int) {
    window_log_max = 100,
};

/// Returns the valid bounds for a decompression parameter.
///
/// This wraps `ZSTD_dParam_getBounds`.
pub fn dParamGetBounds(param: DParameter) ZstdError!Bounds {
    const result = c.ZSTD_dParam_getBounds(@intCast(@intFromEnum(param)));
    if (errors.isError(result.@"error")) {
        return errors.errorCodeToError(@intCast(c.ZSTD_getErrorCode(result.@"error")));
    }
    return .{
        .lower_bound = result.lowerBound,
        .upper_bound = result.upperBound,
    };
}

/// Bounds for a decompression parameter.
pub const Bounds = struct {
    lower_bound: i32,
    upper_bound: i32,
};

/// Reset directives for `Decompressor.reset`.
pub const ResetDirective = enum(c_int) {
    session_only = 1,
    parameters = 2,
    session_and_parameters = 3,
};

/// A decompression context that can be reused across multiple decompression operations.
///
/// When decompressing many times, it is recommended to allocate a context only
/// once and reuse it. Use one context per thread for parallel execution.
pub const Decompressor = struct {
    ctx: *c.ZSTD_DCtx,

    /// Creates a new decompression context.
    ///
    /// This wraps `ZSTD_createDCtx`. The context must be freed with `deinit`.
    pub fn init() !Decompressor {
        const ctx = c.ZSTD_createDCtx() orelse return error.MemoryAllocation;
        return .{ .ctx = ctx };
    }

    /// Frees the decompression context and its associated resources.
    ///
    /// This wraps `ZSTD_freeDCtx`. Safe to call with a null context.
    pub fn deinit(self: Decompressor) void {
        _ = c.ZSTD_freeDCtx(self.ctx);
    }

    /// Sets a decompression parameter on this context.
    ///
    /// This wraps `ZSTD_DCtx_setParameter`. Parameters are sticky and remain
    /// valid for all subsequent frames until the context is reset.
    ///
    /// Setting a parameter is only possible during frame initialization (before
    /// starting decompression).
    pub fn setParameter(self: Decompressor, param: DParameter, value: i32) ZstdError!void {
        _ = try errors.check(c.ZSTD_DCtx_setParameter(
            self.ctx,
            @intCast(@intFromEnum(param)),
            @intCast(value),
        ));
    }

    /// Resets the decompression context.
    ///
    /// This wraps `ZSTD_DCtx_reset`. Use `session_only` to start a new
    /// decompression session, `parameters` to reset all parameters to defaults,
    /// or `session_and_parameters` for both.
    pub fn reset(self: Decompressor, directive: ResetDirective) ZstdError!void {
        _ = try errors.check(c.ZSTD_DCtx_reset(
            self.ctx,
            @intCast(@intFromEnum(directive)),
        ));
    }

    /// Decompresses `src` into `dst` using this context.
    ///
    /// This wraps `ZSTD_decompressDCtx`. Requires an allocated DCtx.
    /// Compatible with sticky parameters.
    pub fn decompress(self: Decompressor, dst: []u8, src: []const u8) ZstdError!usize {
        return errors.check(c.ZSTD_decompressDCtx(
            self.ctx,
            dst.ptr,
            dst.len,
            src.ptr,
            src.len,
        ));
    }

    /// Decompresses `src` into a newly allocated buffer.
    ///
    /// Convenience method that allocates a destination buffer using `expected_size`,
    /// calls `decompress`, and returns the exact-size slice.
    pub fn decompressAlloc(self: Decompressor, allocator: std.mem.Allocator, src: []const u8, expected_size: usize) ZstdError![]u8 {
        const dst = try allocator.alloc(u8, expected_size);
        errdefer allocator.free(dst);

        const written = try self.decompress(dst, src);
        return try allocator.realloc(dst, written);
    }

    /// Loads a dictionary into the decompression context.
    ///
    /// This wraps `ZSTD_DCtx_loadDictionary`. The dictionary is sticky and
    /// will be used for all future frames until explicitly invalidated or
    /// replaced.
    ///
    /// Passing `null` or an empty dictionary invalidates any previous dictionary.
    pub fn loadDictionary(self: Decompressor, dict: ?[]const u8) ZstdError!void {
        const d = dict orelse &[_]u8{};
        _ = try errors.check(c.ZSTD_DCtx_loadDictionary(
            self.ctx,
            d.ptr,
            d.len,
        ));
    }

    /// References a prepared decompression dictionary.
    ///
    /// This wraps `ZSTD_DCtx_refDDict`. The DDict must outlive its usage
    /// within this context. Referencing a null DDict returns to no-dictionary mode.
    pub fn refDDict(self: Decompressor, ddict: ?*const DDict) ZstdError!void {
        _ = try errors.check(c.ZSTD_DCtx_refDDict(
            self.ctx,
            if (ddict) |d| d.ptr else null,
        ));
    }

    /// References a prefix (single-usage dictionary) for the next frame.
    ///
    /// This wraps `ZSTD_DCtx_refPrefix`. A prefix is only used once; reference
    /// is discarded at end of frame. The prefix buffer must outlive decompression.
    pub fn refPrefix(self: Decompressor, prefix: ?[]const u8) ZstdError!void {
        const p = prefix orelse &[_]u8{};
        _ = try errors.check(c.ZSTD_DCtx_refPrefix(
            self.ctx,
            p.ptr,
            p.len,
        ));
    }

    /// Returns the current memory usage of this decompression context.
    ///
    /// This wraps `ZSTD_sizeof_DCtx`.
    pub fn sizeof(self: Decompressor) usize {
        return c.ZSTD_sizeof_DCtx(self.ctx);
    }
};

/// Opaque pointer to a prepared decompression dictionary.
pub const DDict = struct {
    ptr: *c.ZSTD_DDict,

    /// Creates a prepared decompression dictionary from a raw dictionary buffer.
    ///
    /// This wraps `ZSTD_createDDict`. The dictionary content is copied internally,
    /// so `dict_buffer` can be released after creation.
    pub fn init(dict_buffer: []const u8) !DDict {
        const ptr = c.ZSTD_createDDict(
            dict_buffer.ptr,
            dict_buffer.len,
        ) orelse return error.MemoryAllocation;
        return .{ .ptr = ptr };
    }

    /// Frees the decompression dictionary.
    ///
    /// This wraps `ZSTD_freeDDict`.
    pub fn deinit(self: DDict) void {
        _ = c.ZSTD_freeDDict(self.ptr);
    }

    /// Returns the dictionary ID of this decompression dictionary.
    ///
    /// This wraps `ZSTD_getDictID_fromDDict`.
    pub fn getDictId(self: DDict) u32 {
        return @intCast(c.ZSTD_getDictID_fromDDict(self.ptr));
    }

    /// Returns the current memory usage of this dictionary.
    ///
    /// This wraps `ZSTD_sizeof_DDict`.
    pub fn sizeof(self: DDict) usize {
        return c.ZSTD_sizeof_DDict(self.ptr);
    }
};

test "Decompressor init and deinit" {
    var decomp = try Decompressor.init();
    decomp.deinit();
}

test "Decompressor decompress with context" {
    var comp_ctx = try @import("cctx.zig").Compressor.init();
    defer comp_ctx.deinit();

    const original = "Hello, explicit decompression context!";
    var comp_buf = try std.testing.allocator.alloc(u8, 256);
    defer std.testing.allocator.free(comp_buf);

    const comp_len = try comp_ctx.compress2(comp_buf, original);

    var decomp_ctx = try Decompressor.init();
    defer decomp_ctx.deinit();

    var decomp_buf = try std.testing.allocator.alloc(u8, original.len + 64);
    defer std.testing.allocator.free(decomp_buf);

    const decomp_len = try decomp_ctx.decompress(decomp_buf, comp_buf[0..comp_len]);
    try std.testing.expectEqualStrings(original, decomp_buf[0..decomp_len]);
}
