const std = @import("std");
const compress_mod = @import("compress.zig");
const decompress_mod = @import("decompress.zig");
const errors = @import("errors.zig");

pub const ZstdError = errors.ZstdError;

pub const EndDirective = enum(c_int) {
    @"continue" = 0,
    flush = 1,
    end = 2,
};

pub const StreamResult = struct {
    bytes_written: usize,
};

pub const StreamCompressOptions = struct {
    level: compress_mod.CLevel = .default,
    checksum: bool = false,
};

pub const StreamDecompressOptions = struct {
    dict: ?[]const u8 = null,
};

pub fn recommendedCInSize() usize {
    return 128 * 1024;
}

pub fn recommendedCOutSize() usize {
    return 128 * 1024 + 6;
}

pub fn recommendedDInSize() usize {
    return 128 * 1024;
}

pub fn recommendedDOutSize() usize {
    return 128 * 1024;
}

pub const StreamCompressor = struct {
    compression_level: i32,
    checksum: bool,
    frame_started: bool,
    buffer: std.ArrayList(u8),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, opts: StreamCompressOptions) StreamCompressor {
        return .{
            .compression_level = opts.level.toInt(),
            .checksum = opts.checksum,
            .frame_started = false,
            .buffer = .empty,
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *StreamCompressor) void {
        self.buffer.deinit(self.allocator);
        self.* = undefined;
    }

    pub fn reset(self: *StreamCompressor) void {
        self.frame_started = false;
        self.buffer.clearRetainingCapacity();
    }

    pub fn setParameter(self: *StreamCompressor, param: compress_mod.CParameter, value: i32) void {
        switch (param) {
            .compression_level => self.compression_level = value,
            .checksum_flag => self.checksum = value != 0,
            else => {},
        }
    }

    pub fn compressChunk(self: *StreamCompressor, input: []const u8, output: []u8, end_op: EndDirective) ZstdError!StreamResult {
        if (!self.frame_started) {
            self.frame_started = true;
            self.buffer.clearRetainingCapacity();
        }

        if (self.buffer.items.len == 0 and end_op == .end) {
            var cctx = compress_mod.Compressor.init(.{
                .level = compress_mod.CLevel.fromInt(self.compression_level),
                .checksum = self.checksum,
            });
            const written = try cctx.compress2(output, input);
            self.frame_started = false;
            return .{ .bytes_written = written };
        }

        try self.buffer.appendSlice(self.allocator, input);

        if (end_op == .end) {
            var cctx = compress_mod.Compressor.init(.{
                .level = compress_mod.CLevel.fromInt(self.compression_level),
                .checksum = self.checksum,
            });
            const written = try cctx.compress2(output, self.buffer.items);
            self.buffer.clearRetainingCapacity();
            self.frame_started = false;
            return .{ .bytes_written = written };
        }

        return .{ .bytes_written = 0 };
    }

    pub fn endStream(self: *StreamCompressor, output: []u8) ZstdError!StreamResult {
        return self.compressChunk(&.{}, output, .end);
    }

    pub fn flushStream(self: *StreamCompressor, output: []u8) ZstdError!StreamResult {
        _ = self;
        _ = output;
        return .{ .bytes_written = 0 };
    }
};

pub const StreamDecompressor = struct {
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, opts: StreamDecompressOptions) StreamDecompressor {
        _ = opts;
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *StreamDecompressor) void {
        self.* = undefined;
    }

    pub fn decompressChunk(self: *StreamDecompressor, input: []const u8, output: []u8) ZstdError!StreamResult {
        const decompressed = try decompress_mod.decompress(self.allocator, input, .{});
        defer self.allocator.free(decompressed);
        if (decompressed.len > output.len) return error.DstSizeTooSmall;
        @memcpy(output[0..decompressed.len], decompressed);
        return .{ .bytes_written = decompressed.len };
    }
};

test "streaming compress decompress" {
    const allocator = std.testing.allocator;
    const original = "Streaming compression test data for round trip verification";

    var comp = StreamCompressor.init(allocator, .{});
    defer comp.deinit();

    var buf: [4096]u8 = undefined;
    const result = try comp.compressChunk(original, &buf, .end);
    try std.testing.expect(result.bytes_written > 0);

    const compressed = buf[0..result.bytes_written];

    var decomp = StreamDecompressor.init(allocator, .{});
    defer decomp.deinit();

    var out_buf: [4096]u8 = undefined;
    const decomp_result = try decomp.decompressChunk(compressed, &out_buf);

    try std.testing.expectEqualStrings(original, out_buf[0..decomp_result.bytes_written]);
}

test "streaming sizes" {
    try std.testing.expect(recommendedCInSize() > 0);
    try std.testing.expect(recommendedCOutSize() > 0);
    try std.testing.expect(recommendedDInSize() > 0);
    try std.testing.expect(recommendedDOutSize() > 0);
}
