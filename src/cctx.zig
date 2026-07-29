//! Explicit compression context API.
//!
//! Wraps `ZSTD_CCtx`, `ZSTD_CCtx_setParameter`, `ZSTD_CCtx_setPledgedSrcSize`,
//! `ZSTD_CCtx_reset`, `ZSTD_compress2`, `ZSTD_CCtx_loadDictionary`,
//! `ZSTD_CCtx_refCDict`, `ZSTD_CCtx_refPrefix`, and the `ZSTD_cParameter`
//! enum from the Advanced Compression API section of `zstd.h`.

const std = @import("std");
const errors = @import("errors.zig");
const ZstdError = errors.ZstdError;

const c = @cImport({
    @cInclude("zstd.h");
});

/// Compression strategies, listed from fastest to strongest.
pub const Strategy = enum(c_int) {
    fast = 1,
    dfast = 2,
    greedy = 3,
    lazy = 4,
    lazy2 = 5,
    btlazy2 = 6,
    btopt = 7,
    btultra = 8,
    btultra2 = 9,
};

/// Compression parameters that can be set on a `Compressor`.
///
/// Each variant corresponds to a `ZSTD_cParameter` value. The integer
/// embedded in the variant tag matches the C enum value.
pub const CParameter = enum(c_int) {
    compression_level = 100,
    window_log = 101,
    hash_log = 102,
    chain_log = 103,
    search_log = 104,
    min_match = 105,
    target_length = 106,
    strategy = 107,
    target_c_block_size = 130,
    enable_long_distance_matching = 160,
    ldm_hash_log = 161,
    ldm_min_match = 162,
    ldm_bucket_size_log = 163,
    ldm_hash_rate_log = 164,
    content_size_flag = 200,
    checksum_flag = 201,
    dict_id_flag = 202,
    nb_workers = 400,
    job_size = 401,
    overlap_log = 402,
};

/// Reset directives for `Compressor.reset`.
pub const ResetDirective = enum(c_int) {
    session_only = 1,
    parameters = 2,
    session_and_parameters = 3,
};

/// Bounds for a compression or decompression parameter.
pub const Bounds = struct {
    lower_bound: i32,
    upper_bound: i32,
};

/// Returns the valid bounds for a compression parameter.
///
/// This wraps `ZSTD_cParam_getBounds`.
pub fn cParamGetBounds(param: CParameter) ZstdError!Bounds {
    const result = c.ZSTD_cParam_getBounds(@intCast(@intFromEnum(param)));
    if (errors.isError(result.@"error")) {
        return errors.errorCodeToError(@intCast(c.ZSTD_getErrorCode(result.@"error")));
    }
    return .{
        .lower_bound = result.lowerBound,
        .upper_bound = result.upperBound,
    };
}

/// A compression context that can be reused across multiple compression operations.
///
/// When compressing many times, it is recommended to allocate a compression
/// context just once and reuse it for each successive compression operation.
/// This will make the workload easier for the system's memory.
///
/// Re-using context is a speed/resource optimization; it does not change the
/// compression ratio. For parallel execution, use one context per thread.
pub const Compressor = struct {
    ctx: *c.ZSTD_CCtx,

    /// Creates a new compression context.
    ///
    /// This wraps `ZSTD_createCCtx`. The context must be freed with `deinit`.
    pub fn init() !Compressor {
        const ctx = c.ZSTD_createCCtx() orelse return error.MemoryAllocation;
        return .{ .ctx = ctx };
    }

    /// Frees the compression context and its associated resources.
    ///
    /// This wraps `ZSTD_freeCCtx`. Safe to call with a null context.
    pub fn deinit(self: Compressor) void {
        _ = c.ZSTD_freeCCtx(self.ctx);
    }

    /// Sets a compression parameter on this context.
    ///
    /// This wraps `ZSTD_CCtx_setParameter`. Parameters are sticky and remain
    /// valid for all subsequent frames until the context is reset.
    ///
    /// Some parameters can only be set before compression starts; others (like
    /// `compression_level`, `hash_log`, `chain_log`) can be updated during
    /// multi-threaded compression (when `nb_workers >= 1`).
    pub fn setParameter(self: Compressor, param: CParameter, value: i32) ZstdError!void {
        _ = try errors.check(c.ZSTD_CCtx_setParameter(
            self.ctx,
            @intCast(@intFromEnum(param)),
            @intCast(value),
        ));
    }

    /// Sets the total input data size to be compressed as a single frame.
    ///
    /// This wraps `ZSTD_CCtx_setPledgedSrcSize`. The value will be written
    /// into the frame header (unless disabled via `content_size_flag`). Pass
    /// `ZSTD_CONTENTSIZE_UNKNOWN` if the size is unknown.
    pub fn setPledgedSrcSize(self: Compressor, src_size: u64) ZstdError!void {
        _ = try errors.check(c.ZSTD_CCtx_setPledgedSrcSize(self.ctx, src_size));
    }

    /// Resets the compression context.
    ///
    /// This wraps `ZSTD_CCtx_reset`. Use `session_only` to start a new frame,
    /// `parameters` to reset all parameters to defaults, or
    /// `session_and_parameters` for both.
    pub fn reset(self: Compressor, directive: ResetDirective) ZstdError!void {
        _ = try errors.check(c.ZSTD_CCtx_reset(
            self.ctx,
            @intCast(@intFromEnum(directive)),
        ));
    }

    /// Compresses `src` into `dst` using the compression parameters set on this context.
    ///
    /// This wraps `ZSTD_compress2`. The function always starts a new frame.
    /// `dst.len` must be >= `compressBound(src.len)` to guarantee success.
    pub fn compress2(self: Compressor, dst: []u8, src: []const u8) ZstdError!usize {
        return errors.check(c.ZSTD_compress2(
            self.ctx,
            dst.ptr,
            dst.len,
            src.ptr,
            src.len,
        ));
    }

    /// Compresses `src` into a newly allocated buffer.
    ///
    /// Convenience method that allocates a destination buffer using `compressBound`,
    /// calls `compress2`, and returns the exact-size slice.
    pub fn compressAlloc(self: Compressor, allocator: std.mem.Allocator, src: []const u8) ZstdError![]u8 {
        const bound = try errors.check(c.ZSTD_compressBound(src.len));
        const dst = try allocator.alloc(u8, bound);
        errdefer allocator.free(dst);

        const written = try self.compress2(dst, src);
        return try allocator.realloc(dst, written);
    }

    /// Loads a dictionary into the compression context.
    ///
    /// This wraps `ZSTD_CCtx_loadDictionary`. The dictionary is sticky and
    /// will be used for all future compressed frames until parameters are reset
    /// or a new dictionary is loaded.
    ///
    /// Passing `null` or an empty dictionary invalidates any previous dictionary.
    pub fn loadDictionary(self: Compressor, dict: ?[]const u8) ZstdError!void {
        const d = dict orelse &[_]u8{};
        _ = try errors.check(c.ZSTD_CCtx_loadDictionary(
            self.ctx,
            d.ptr,
            d.len,
        ));
    }

    /// References a prepared compression dictionary.
    ///
    /// This wraps `ZSTD_CCtx_refCDict`. The CDict must outlive its usage
    /// within this context. Referencing a null CDict returns to no-dictionary mode.
    pub fn refCDict(self: Compressor, cdict: ?*const CDict) ZstdError!void {
        _ = try errors.check(c.ZSTD_CCtx_refCDict(
            self.ctx,
            if (cdict) |d| d.ptr else null,
        ));
    }

    /// References a prefix (single-usage dictionary) for the next compressed frame.
    ///
    /// This wraps `ZSTD_CCtx_refPrefix`. A prefix is only used once; tables
    /// are discarded at end of frame. The prefix buffer must outlive compression
    /// and its content must remain unmodified during compression.
    pub fn refPrefix(self: Compressor, prefix: ?[]const u8) ZstdError!void {
        const p = prefix orelse &[_]u8{};
        _ = try errors.check(c.ZSTD_CCtx_refPrefix(
            self.ctx,
            p.ptr,
            p.len,
        ));
    }

    /// Returns the current memory usage of this compression context.
    ///
    /// This wraps `ZSTD_sizeof_CCtx`.
    pub fn sizeof(self: Compressor) usize {
        return c.ZSTD_sizeof_CCtx(self.ctx);
    }
};

/// Opaque pointer to a prepared compression dictionary.
pub const CDict = struct {
    ptr: *c.ZSTD_CDict,

    /// Creates a prepared compression dictionary from a raw dictionary buffer.
    ///
    /// This wraps `ZSTD_createCDict`. The dictionary content is copied internally,
    /// so `dict_buffer` can be released after creation. The compression level
    /// is set at dictionary creation time.
    pub fn init(dict_buffer: []const u8, compression_level: i32) !CDict {
        const ptr = c.ZSTD_createCDict(
            dict_buffer.ptr,
            dict_buffer.len,
            @intCast(compression_level),
        ) orelse return error.MemoryAllocation;
        return .{ .ptr = ptr };
    }

    /// Frees the compression dictionary.
    ///
    /// This wraps `ZSTD_freeCDict`.
    pub fn deinit(self: CDict) void {
        _ = c.ZSTD_freeCDict(self.ptr);
    }

    /// Returns the dictionary ID of this compression dictionary.
    ///
    /// This wraps `ZSTD_getDictID_fromCDict`.
    pub fn getDictId(self: CDict) u32 {
        return @intCast(c.ZSTD_getDictID_fromCDict(self.ptr));
    }

    /// Returns the current memory usage of this dictionary.
    ///
    /// This wraps `ZSTD_sizeof_CDict`.
    pub fn sizeof(self: CDict) usize {
        return c.ZSTD_sizeof_CDict(self.ptr);
    }
};

test "Compressor init and deinit" {
    var comp = try Compressor.init();
    comp.deinit();
}

test "Compressor setParameter and compress" {
    var comp = try Compressor.init();
    defer comp.deinit();

    try comp.setParameter(.compression_level, 1);

    const original = "This string is long enough for zstd to compress effectively and produce smaller output. " ** 3;
    const bound = try errors.check(c.ZSTD_compressBound(original.len));
    const dst = try std.testing.allocator.alloc(u8, bound);
    defer std.testing.allocator.free(dst);

    const written = try comp.compress2(dst, original);
    try std.testing.expect(written > 0);
    try std.testing.expect(written < original.len);
}

test "CDict creation" {
    const dict_data = "some dictionary content for testing purposes that is long enough";
    var cdict = try CDict.init(dict_data, 3);
    defer cdict.deinit();

    const dict_id = cdict.getDictId();
    _ = dict_id;
}
