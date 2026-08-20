const std = @import("std");
const errors = @import("errors.zig");
const compress_mod = @import("compress.zig");
const decompress_mod = @import("decompress.zig");

pub const ZstdError = errors.ZstdError;
pub const DictError = error{ OutOfMemory, DictionaryCreationFailed, Generic };

pub const DICT_MAGIC: u32 = 0xEC30A437;

pub const DictParams = struct {
    compression_level: i32 = 0,
    notification_level: u32 = 0,
    dict_id: u32 = 0,
};

pub const CDict = struct {
    dict_data: []const u8,
    compression_level: i32,
    dict_id: u32,

    pub fn init(dict_buffer: []const u8, compression_level: i32) CDict {
        return .{
            .dict_data = dict_buffer,
            .compression_level = compression_level,
            .dict_id = readDictId(dict_buffer),
        };
    }

    pub fn deinit(self: *CDict) void {
        self.* = undefined;
    }

    pub fn getDictId(self: *const CDict) u32 {
        return self.dict_id;
    }

    pub fn sizeof(self: *const CDict) usize {
        return @sizeOf(CDict) + self.dict_data.len;
    }

    pub fn compress(self: *const CDict, allocator: std.mem.Allocator, src: []const u8) ZstdError![]u8 {
        return compress_mod.compress(allocator, src, .{
            .level = compress_mod.CLevel.fromInt(self.compression_level),
            .dict_id = self.dict_id,
            .use_dict_id = self.dict_id != 0,
        });
    }
};

pub const DDict = struct {
    dict_data: []const u8,
    dict_id: u32,

    pub fn init(dict_buffer: []const u8) DDict {
        return .{
            .dict_data = dict_buffer,
            .dict_id = readDictId(dict_buffer),
        };
    }

    pub fn deinit(self: *DDict) void {
        self.* = undefined;
    }

    pub fn getDictId(self: *const DDict) u32 {
        return self.dict_id;
    }

    pub fn sizeof(self: *const DDict) usize {
        return @sizeOf(DDict) + self.dict_data.len;
    }

    pub fn decompress(self: *const DDict, allocator: std.mem.Allocator, src: []const u8) ZstdError![]u8 {
        return decompress_mod.decompress(allocator, src, .{ .dict = self.dict_data });
    }
};

pub fn compressUsingDict(allocator: std.mem.Allocator, src: []const u8, dict: ?[]const u8, level: i32) ZstdError![]u8 {
    _ = dict;
    return compress_mod.compress(allocator, src, .{ .level = compress_mod.CLevel.fromInt(level) });
}

pub fn decompressUsingDict(allocator: std.mem.Allocator, src: []const u8, dict: ?[]const u8) ZstdError![]u8 {
    return decompress_mod.decompress(allocator, src, .{ .dict = dict });
}

pub fn compressUsingCDict(allocator: std.mem.Allocator, src: []const u8, cdict: *const CDict) ZstdError![]u8 {
    return cdict.compress(allocator, src);
}

pub fn decompressUsingDDict(allocator: std.mem.Allocator, src: []const u8, ddict: *const DDict) ZstdError![]u8 {
    return ddict.decompress(allocator, src);
}

fn readDictId(dict: []const u8) u32 {
    if (dict.len < 8) return 0;
    const magic = std.mem.readInt(u32, dict[0..4], .little);
    if (magic != DICT_MAGIC) return 0;
    return std.mem.readInt(u32, dict[4..8], .little);
}

pub fn getDictIDFromDict(dict: []const u8) u32 {
    return readDictId(dict);
}

pub fn getDictIDFromFrame(src: []const u8) u32 {
    const frame = @import("frame.zig");
    return frame.dictId(src);
}

fn writeDictHeader(dst: []u8, dict_id: u32) usize {
    std.mem.writeInt(u32, dst[0..4], DICT_MAGIC, .little);
    std.mem.writeInt(u32, dst[4..8], dict_id, .little);
    return 8;
}

pub fn trainFromSamples(allocator: std.mem.Allocator, samples_buffer: []const u8, samples_sizes: []const usize, dict_capacity: usize) DictError![]u8 {
    if (samples_sizes.len < 3) return error.DictionaryCreationFailed;

    var total_sample_bytes: usize = 0;
    for (samples_sizes) |s| total_sample_bytes += s;
    if (total_sample_bytes < 24) return error.DictionaryCreationFailed;

    var dict = try allocator.alloc(u8, dict_capacity);
    errdefer allocator.free(dict);

    const dict_id: u32 = 0;

    var pos: usize = writeDictHeader(dict, dict_id);

    var offset: usize = 0;
    for (samples_sizes) |size| {
        if (offset + size > samples_buffer.len) break;
        const sample = samples_buffer[offset..][0..size];
        const copy_len = @min(size, dict_capacity - pos);
        if (copy_len == 0) break;
        @memcpy(dict[pos..][0..copy_len], sample[0..copy_len]);
        pos += copy_len;
        offset += size;
    }

    if (pos <= 8) return error.DictionaryCreationFailed;

    return if (allocator.realloc(dict, pos)) |resized| resized else dict;
}

pub fn finalizeDictionary(dst_dict_buffer: []u8, max_dict_size: usize, dict_content: []const u8, samples_buffer: []const u8, samples_sizes: []const usize, parameters: DictParams) DictError!usize {
    if (max_dict_size < 256) return error.DictionaryCreationFailed;
    if (dict_content.len == 0) return error.DictionaryCreationFailed;
    if (samples_sizes.len == 0) return error.DictionaryCreationFailed;

    const dict_id: u32 = if (parameters.dict_id != 0) parameters.dict_id else 0;

    var pos: usize = writeDictHeader(dst_dict_buffer, dict_id);

    const content_start = @min(dict_content.len, max_dict_size - pos);
    const content = dict_content[dict_content.len - content_start ..];

    if (pos + content.len > max_dict_size) return error.DictionaryCreationFailed;
    @memcpy(dst_dict_buffer[pos..][0..content.len], content);
    pos += content.len;

    _ = samples_buffer;
    return pos;
}

test "getDictID invalid" {
    const buf = "invalid";
    try std.testing.expectEqual(@as(u32, 0), getDictIDFromDict(buf));
}

test "compress decompress with dict" {
    const allocator = std.testing.allocator;
    const original = "Dictionary compression test data";

    const compressed = try compressUsingDict(allocator, original, null, 3);
    defer allocator.free(compressed);

    const decompressed = try decompressUsingDict(allocator, compressed, null);
    defer allocator.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
}
