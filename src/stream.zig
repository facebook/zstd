//! Streaming compression and decompression API.
//!
//! Wraps `ZSTD_compressStream2`, `ZSTD_decompressStream`, `ZSTD_CStreamInSize`,
//! `ZSTD_CStreamOutSize`, `ZSTD_DStreamInSize`, `ZSTD_DStreamOutSize`,
//! `ZSTD_endStream`, `ZSTD_flushStream`, `ZSTD_initCStream`, `ZSTD_initDStream`,
//! and the `ZSTD_inBuffer`/`ZSTD_outBuffer` types from the Streaming section
//! of `zstd.h`.

const std = @import("std");
const errors = @import("errors.zig");
const ZstdError = errors.ZstdError;

const c = @cImport({
    @cInclude("zstd.h");
});

/// End directives for streaming compression.
pub const EndDirective = enum(c_int) {
    @"continue" = 0,
    flush = 1,
    end = 2,
};

/// Result of a single streaming compression or decompression operation.
pub const StreamResult = struct {
    /// Number of bytes written to the output buffer.
    bytes_written: usize,
    /// Number of bytes remaining to be flushed from internal buffers.
    /// When `0`, the operation is complete.
    remaining: usize,
};

/// Recommended size for streaming compression input buffer.
pub fn cStreamInSize() usize {
    return c.ZSTD_CStreamInSize();
}

/// Recommended size for streaming compression output buffer.
///
/// Guarantees successful flush of at least one complete compressed block.
pub fn cStreamOutSize() usize {
    return c.ZSTD_CStreamOutSize();
}

/// Recommended size for streaming decompression input buffer.
pub fn dStreamInSize() usize {
    return c.ZSTD_DStreamInSize();
}

/// Recommended size for streaming decompression output buffer.
///
/// Guarantees successful flush of at least one complete block in all circumstances.
pub fn dStreamOutSize() usize {
    return c.ZSTD_DStreamOutSize();
}

/// A streaming compressor that wraps `ZSTD_CCtx` (which is the same as
/// `ZSTD_CStream` since v1.3.0).
///
/// Use this for compressing data that is too large to fit in memory at once.
/// Feed data in chunks via `compressChunk` and collect compressed output.
pub const StreamingCompressor = struct {
    ctx: *c.ZSTD_CCtx,

    /// Creates a new streaming compressor.
    ///
    /// This wraps `ZSTD_createCStream`.
    pub fn init() !StreamingCompressor {
        const ctx = c.ZSTD_createCStream() orelse return error.MemoryAllocation;
        return .{ .ctx = ctx };
    }

    /// Frees the streaming compressor.
    ///
    /// This wraps `ZSTD_freeCStream`.
    pub fn deinit(self: StreamingCompressor) void {
        _ = c.ZSTD_freeCStream(self.ctx);
    }

    /// Initializes the streaming compressor for a new compression operation.
    ///
    /// This wraps `ZSTD_initCStream`. Equivalent to resetting the session and
    /// setting the compression level.
    pub fn initStream(self: StreamingCompressor, compression_level: i32) ZstdError!void {
        _ = try errors.check(c.ZSTD_initCStream(self.ctx, @intCast(compression_level)));
    }

    /// Resets the streaming compressor session.
    ///
    /// This wraps `ZSTD_CCtx_reset` with `ZSTD_reset_session_only`.
    pub fn reset(self: StreamingCompressor) ZstdError!void {
        _ = try errors.check(c.ZSTD_CCtx_reset(self.ctx, 1));
    }

    /// Compresses a chunk of data.
    ///
    /// This wraps `ZSTD_compressStream2`. The caller must loop until
    /// `remaining == 0` to ensure all data has been flushed from internal buffers.
    ///
    /// `end_op` controls the behavior:
    /// - `.@"continue"`: collect more data, encoder decides when to output
    /// - `.flush`: flush any data provided so far
    /// - `.end`: flush remaining data and close the current frame
    pub fn compressChunk(
        self: StreamingCompressor,
        input: []const u8,
        output: []u8,
        end_op: EndDirective,
    ) ZstdError!StreamResult {
        var in_buf = c.ZSTD_inBuffer{
            .src = input.ptr,
            .size = input.len,
            .pos = 0,
        };
        var out_buf = c.ZSTD_outBuffer{
            .dst = output.ptr,
            .size = output.len,
            .pos = 0,
        };

        const remaining = try errors.check(c.ZSTD_compressStream2(
            self.ctx,
            &out_buf,
            &in_buf,
            @intCast(@intFromEnum(end_op)),
        ));

        return .{ .bytes_written = out_buf.pos, .remaining = remaining };
    }

    /// Ends the current compression frame.
    ///
    /// This wraps `ZSTD_endStream`. Call in a loop until `remaining == 0`.
    pub fn endStream(self: StreamingCompressor, output: []u8) ZstdError!StreamResult {
        var out_buf = c.ZSTD_outBuffer{
            .dst = output.ptr,
            .size = output.len,
            .pos = 0,
        };

        const remaining = try errors.check(c.ZSTD_endStream(self.ctx, &out_buf));
        return .{ .bytes_written = out_buf.pos, .remaining = remaining };
    }

    /// Flushes any data that might remain stuck within internal buffers.
    ///
    /// This wraps `ZSTD_flushStream`. Call in a loop until `remaining == 0`.
    pub fn flushStream(self: StreamingCompressor, output: []u8) ZstdError!StreamResult {
        var out_buf = c.ZSTD_outBuffer{
            .dst = output.ptr,
            .size = output.len,
            .pos = 0,
        };

        const remaining = try errors.check(c.ZSTD_flushStream(self.ctx, &out_buf));
        return .{ .bytes_written = out_buf.pos, .remaining = remaining };
    }

    /// Sets a compression parameter on the streaming compressor.
    pub fn setParameter(self: StreamingCompressor, param: @import("cctx.zig").CParameter, value: i32) ZstdError!void {
        _ = try errors.check(c.ZSTD_CCtx_setParameter(
            self.ctx,
            @intCast(@intFromEnum(param)),
            @intCast(value),
        ));
    }
};

/// A streaming decompressor that wraps `ZSTD_DCtx` (which is the same as
/// `ZSTD_DStream` since v1.3.0).
///
/// Use this for decompressing data that is too large to fit in memory at once.
/// Feed compressed data in chunks via `decompressChunk` and collect decompressed output.
pub const StreamingDecompressor = struct {
    ctx: *c.ZSTD_DCtx,

    /// Creates a new streaming decompressor.
    ///
    /// This wraps `ZSTD_createDStream`.
    pub fn init() !StreamingDecompressor {
        const ctx = c.ZSTD_createDStream() orelse return error.MemoryAllocation;
        return .{ .ctx = ctx };
    }

    /// Frees the streaming decompressor.
    ///
    /// This wraps `ZSTD_freeDStream`.
    pub fn deinit(self: StreamingDecompressor) void {
        _ = c.ZSTD_freeDStream(self.ctx);
    }

    /// Initializes the streaming decompressor for a new decompression operation.
    ///
    /// This wraps `ZSTD_initDStream`.
    pub fn initStream(self: StreamingDecompressor) ZstdError!void {
        _ = try errors.check(c.ZSTD_initDStream(self.ctx));
    }

    /// Decompresses a chunk of data.
    ///
    /// This wraps `ZSTD_decompressStream`. The caller must loop until
    /// `remaining == 0` to ensure the frame is fully decoded.
    ///
    /// When `remaining > 0`, either more input is needed or more output space
    /// is required. When `remaining == 0`, the frame is completely decoded and
    /// fully flushed.
    pub fn decompressChunk(
        self: StreamingDecompressor,
        input: []const u8,
        output: []u8,
    ) ZstdError!StreamResult {
        var in_buf = c.ZSTD_inBuffer{
            .src = input.ptr,
            .size = input.len,
            .pos = 0,
        };
        var out_buf = c.ZSTD_outBuffer{
            .dst = output.ptr,
            .size = output.len,
            .pos = 0,
        };

        const remaining = try errors.check(c.ZSTD_decompressStream(
            self.ctx,
            &out_buf,
            &in_buf,
        ));

        return .{ .bytes_written = out_buf.pos, .remaining = remaining };
    }
};

test "StreamingCompressor init and deinit" {
    var scomp = try StreamingCompressor.init();
    scomp.deinit();
}

test "StreamingDecompressor init and deinit" {
    var sdecomp = try StreamingDecompressor.init();
    sdecomp.deinit();
}

test "streaming round trip" {
    const original = "Streaming compression test data that should round-trip correctly through the streaming API with multiple chunks";

    var scomp = try StreamingCompressor.init();
    defer scomp.deinit();

    try scomp.setParameter(.compression_level, 3);

    var comp_buf = try std.testing.allocator.alloc(u8, 512);
    defer std.testing.allocator.free(comp_buf);

    const res = try scomp.compressChunk(original, comp_buf, .end);
    _ = res;

    var sdecomp = try StreamingDecompressor.init();
    defer sdecomp.deinit();

    const decomp_buf = try std.testing.allocator.alloc(u8, 256);
    defer std.testing.allocator.free(decomp_buf);

    const dres = try sdecomp.decompressChunk(comp_buf[0..comp_buf.len], decomp_buf);
    try std.testing.expect(dres.remaining == 0);
}
