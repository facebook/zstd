const std = @import("std");

pub const errors = @import("errors.zig");
pub const compress_mod = @import("compress.zig");
pub const decompress_mod = @import("decompress.zig");
pub const streaming_mod = @import("streaming.zig");
pub const dict_mod = @import("dict.zig");
pub const frame_mod = @import("frame.zig");
pub const constants_mod = @import("constants.zig");
pub const version_mod = @import("version.zig");

// ── Error types ──────────────────────────────────────────────────
pub const ZstdError = errors.ZstdError;
pub const ErrorCode = errors.ErrorCode;
pub const Error = ZstdError;

// ── Compression level ────────────────────────────────────────────
pub const CLevel = compress_mod.CLevel;

// ── Options structs ──────────────────────────────────────────────
pub const CompressOptions = compress_mod.CompressOptions;
pub const DecompressOptions = decompress_mod.DecompressOptions;

// ── Reusable compressor / decompressor ───────────────────────────
pub const Compressor = compress_mod.Compressor;
pub const Decompressor = struct {
    allocator: std.mem.Allocator,
    opts: DecompressOptions,

    pub fn init(allocator: std.mem.Allocator, opts: DecompressOptions) Decompressor {
        return .{ .allocator = allocator, .opts = opts };
    }

    pub fn deinit(self: *Decompressor) void {
        self.* = undefined;
    }

    pub fn decompress(self: *Decompressor, src: []const u8) ZstdError![]u8 {
        return decompress_mod.decompress(self.allocator, src, self.opts);
    }
};

// ── Streaming ────────────────────────────────────────────────────
pub const StreamCompressor = streaming_mod.StreamCompressor;
pub const StreamDecompressor = streaming_mod.StreamDecompressor;
pub const StreamCompressOptions = streaming_mod.StreamCompressOptions;
pub const StreamDecompressOptions = streaming_mod.StreamDecompressOptions;
pub const EndDirective = streaming_mod.EndDirective;

pub const StreamResult = streaming_mod.StreamResult;

// ── Dictionary types ─────────────────────────────────────────────
pub const CDict = dict_mod.CDict;
pub const DDict = dict_mod.DDict;
pub const DictParams = dict_mod.DictParams;

// ── CParameter, Strategy, ResetDirective ─────────────────────────
pub const CParameter = compress_mod.CParameter;
pub const Strategy = compress_mod.Strategy;
pub const ResetDirective = compress_mod.ResetDirective;
pub const Bounds = compress_mod.Bounds;

// ── Frame namespace ──────────────────────────────────────────────
pub const Frame = struct {
    pub const isFrame = frame_mod.isFrame;
    pub const contentSize = frame_mod.contentSize;
    pub const compressedSize = frame_mod.compressedSize;
    pub const dictId = frame_mod.dictId;
    pub const inspect = frame_mod.inspect;
    pub const ContentSizeResult = frame_mod.ContentSizeResult;
    pub const FrameInfo = frame_mod.FrameInfo;
};

// ── DParameter (decompression parameters) ───────────────────────
pub const DParameter = decompress_mod.DParameter;
pub const dParamGetBounds = decompress_mod.dParamGetBounds;

// ── Version namespace ────────────────────────────────────────────
pub const version = struct {
    pub const number = version_mod.number;
    pub const string = version_mod.string;
    pub const major = version_mod.major;
    pub const minor = version_mod.minor;
    pub const release = version_mod.release;
    pub const clevel_default = version_mod.clevel_default;
    pub const clevel_min = version_mod.clevel_min;
    pub const clevel_max = version_mod.clevel_max;
};

// ── Constants namespace ──────────────────────────────────────────
pub const constants = struct {
    pub const magic_number = constants_mod.magic_number;
    pub const magic_dictionary = constants_mod.magic_dictionary;
    pub const magic_skippable_start = constants_mod.magic_skippable_start;
    pub const magic_skippable_mask = constants_mod.magic_skippable_mask;
    pub const block_size_log_max = constants_mod.block_size_log_max;
    pub const block_size_max = constants_mod.block_size_max;
    pub const content_size_unknown = constants_mod.content_size_unknown;
    pub const content_size_error = constants_mod.content_size_error;
    pub const max_input_size = constants_mod.max_input_size;
};

// ── Top-level functions ──────────────────────────────────────────
pub fn compress(allocator: std.mem.Allocator, src: []const u8, opts: CompressOptions) ZstdError![]u8 {
    return compress_mod.compress(allocator, src, opts);
}

pub fn decompress(allocator: std.mem.Allocator, src: []const u8, opts: DecompressOptions) ZstdError![]u8 {
    return decompress_mod.decompress(allocator, src, opts);
}

pub fn compressBound(src_size: usize) ZstdError!usize {
    return compress_mod.compressBound(src_size);
}

// ── Dictionary convenience functions ─────────────────────────────
pub const compressUsingDict = dict_mod.compressUsingDict;
pub const decompressUsingDict = dict_mod.decompressUsingDict;
pub const compressUsingCDict = dict_mod.compressUsingCDict;
pub const decompressUsingDDict = dict_mod.decompressUsingDDict;
pub const getDictIDFromDict = dict_mod.getDictIDFromDict;
pub const getDictIDFromFrame = dict_mod.getDictIDFromFrame;
pub const trainFromSamples = dict_mod.trainFromSamples;
pub const finalizeDictionary = dict_mod.finalizeDictionary;

// ── Streaming convenience functions ──────────────────────────────
pub const recommendedCInSize = streaming_mod.recommendedCInSize;
pub const recommendedCOutSize = streaming_mod.recommendedCOutSize;
pub const recommendedDInSize = streaming_mod.recommendedDInSize;
pub const recommendedDOutSize = streaming_mod.recommendedDOutSize;

// ── CParameter bounds ────────────────────────────────────────────
pub const cParamGetBounds = compress_mod.cParamGetBounds;

// ── Backward-compatible aliases ──────────────────────────────────
pub const VERSION_NUMBER = version.number;
pub const VERSION_STRING = version.string;
pub const CLEVEL_DEFAULT = version.clevel_default;
pub const MAGIC_NUMBER = constants.magic_number;
pub const BLOCKSIZE_MAX = constants.block_size_max;
pub const CONTENTSIZE_UNKNOWN = constants.content_size_unknown;
pub const ContentSizeResult = frame_mod.ContentSizeResult;

pub fn versionNumber() u32 {
    return version.number;
}

pub fn versionString() []const u8 {
    return version.string;
}

pub fn minCLevel() i32 {
    return version.clevel_min;
}

pub fn maxCLevel() i32 {
    return version.clevel_max;
}

pub fn defaultCLevel() i32 {
    return version.clevel_default;
}

pub fn isFrame(src: []const u8) bool {
    return Frame.isFrame(src);
}

pub fn getFrameContentSize(src: []const u8) ContentSizeResult {
    return Frame.contentSize(src);
}

pub fn findFrameCompressedSize(src: []const u8) ZstdError!usize {
    return Frame.compressedSize(src);
}

// ── Tests ────────────────────────────────────────────────────────
test {
    _ = errors;
    _ = compress_mod;
    _ = decompress_mod;
    _ = streaming_mod;
    _ = dict_mod;
    _ = frame_mod;
    _ = constants_mod;
    _ = version_mod;
}

test "compress decompress round trip" {
    const allocator = std.testing.allocator;
    const original = "zstd.zig native Zig implementation - full round trip";

    const compressed = try compress(allocator, original, .{});
    defer allocator.free(compressed);

    const decompressed = try decompress(allocator, compressed, .{});
    defer allocator.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
}

test "version info" {
    try std.testing.expectEqual(@as(u32, 10600), version.number);
    try std.testing.expectEqualStrings("1.6.0", version.string);
}

test "compressor lifecycle" {
    var comp = Compressor.init(.{});
    defer comp.deinit();

    try comp.setParameter(.compression_level, 5);
    try std.testing.expectEqual(@as(i32, 5), comp.level);
}

test "frame detection" {
    const allocator = std.testing.allocator;
    const data = "Frame detection test";
    const compressed = try compress(allocator, data, .{});
    defer allocator.free(compressed);

    try std.testing.expect(Frame.isFrame(compressed));
    try std.testing.expect(!Frame.isFrame("not a frame"));
}

test "CLevel enum values" {
    try std.testing.expectEqual(@as(i32, 1), CLevel.fastest.toInt());
    try std.testing.expectEqual(@as(i32, 3), CLevel.default.toInt());
    try std.testing.expectEqual(@as(i32, 19), CLevel.best.toInt());
}

test "CompressOptions with level" {
    const compressed = try compress(std.testing.allocator, "test", .{ .level = .fastest });
    defer std.testing.allocator.free(compressed);
    try std.testing.expect(compressed.len > 0);
}

test "Decompressor reusable context" {
    const allocator = std.testing.allocator;
    var decomp = Decompressor.init(allocator, .{});
    defer decomp.deinit();

    const original = "reusable decompressor test";
    const compressed = try compress(allocator, original, .{});
    defer allocator.free(compressed);

    const decompressed = try decomp.decompress(compressed);
    defer allocator.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
}
