const std = @import("std");
const constants = @import("constants.zig");
const errors = @import("errors.zig");

pub const ZstdError = errors.ZstdError;

pub const ContentSizeResult = union(enum) {
    known: u64,
    unknown,
    @"error",
};

pub const FrameInfo = struct {
    content_size: ?u64,
    window_size: ?u64,
    dictionary_id: ?u32,
    checksum: bool,
    single_segment: bool,
    fcs_flag: u2,
};

pub fn isFrame(src: []const u8) bool {
    if (src.len < 4) return false;
    const magic = std.mem.readInt(u32, src[0..4], .little);
    return magic == constants.magic_number;
}

pub fn inspect(src: []const u8) ?FrameInfo {
    if (src.len < 5) return null;
    const magic = std.mem.readInt(u32, src[0..4], .little);
    if (magic != constants.magic_number) return null;

    const descriptor = src[4];
    const fcs_flag: u2 = @truncate(descriptor & 0x3);
    const has_checksum = (descriptor & 0x4) != 0;
    const single_segment = (descriptor & 0x40) != 0;
    const dict_id_flag: u2 = @truncate((descriptor >> 3) & 0x3);

    var pos: usize = 5;
    var window_size: ?u64 = null;

    if (!single_segment) {
        if (pos >= src.len) return null;
        const wd = src[pos];
        pos += 1;
        const exp: u6 = @intCast(wd & 0x0F);
        const mantissa: u32 = @as(u32, 1) << @intCast(3 + (wd >> 3));
        window_size = (@as(u64, 1) << @intCast(exp)) + @as(u64, mantissa);
    }

    var dict_id: ?u32 = null;
    if (dict_id_flag > 0) {
        const field_size: usize = if (dict_id_flag == 3) 4 else @as(usize, 1) << @intCast(dict_id_flag - 1);
        if (pos + field_size > src.len) return null;
        if (field_size >= 4) {
            dict_id = std.mem.readInt(u32, src[pos..][0..4], .little);
        }
        pos += field_size;
    }

    var content_size: ?u64 = null;
    if (fcs_flag > 0 or single_segment) {
        const field_size: usize = if (fcs_flag == 0) @as(usize, 0) else @as(usize, 1) << @intCast(fcs_flag);
        if (fcs_flag > 0) {
            if (pos + field_size > src.len) return null;
            const fcs_val: u64 = switch (fcs_flag) {
                1 => src[pos],
                2 => std.mem.readInt(u16, src[pos..][0..2], .little),
                3 => std.mem.readInt(u64, src[pos..][0..8], .little),
                else => 0,
            };
            if (fcs_val != constants.content_size_unknown and fcs_val != constants.content_size_error) {
                content_size = fcs_val;
            }
        }
        pos += field_size;
    }

    return .{
        .content_size = content_size,
        .window_size = window_size,
        .dictionary_id = dict_id,
        .checksum = has_checksum,
        .single_segment = single_segment,
        .fcs_flag = fcs_flag,
    };
}

pub fn contentSize(src: []const u8) ContentSizeResult {
    if (src.len < 13) return .@"error";
    const magic = std.mem.readInt(u32, src[0..4], .little);
    if (magic != constants.magic_number) return .@"error";

    const descriptor = src[4];
    const fcs_flag = descriptor & 0x3;
    const single_segment = (descriptor & 0x40) != 0;

    if (fcs_flag == 0 and !single_segment) return .unknown;

    var base: usize = 5;
    if (!single_segment) base += 1;

    const dict_id_flag = (descriptor >> 3) & 0x3;
    if (dict_id_flag > 0) {
        const field_size: usize = if (dict_id_flag == 3) 4 else @as(usize, 1) << @intCast(dict_id_flag - 1);
        base += field_size;
    }

    if (fcs_flag == 0 and single_segment) {
        return .unknown;
    }

    switch (fcs_flag) {
        1 => {
            if (src.len < base + 1) return .@"error";
            return .{ .known = src[base] };
        },
        2 => {
            if (src.len < base + 2) return .@"error";
            return .{ .known = std.mem.readInt(u16, src[base..][0..2], .little) };
        },
        3 => {
            if (src.len < base + 8) return .@"error";
            const fcs = std.mem.readInt(u64, src[base..][0..8], .little);
            if (fcs == constants.content_size_unknown) return .unknown;
            if (fcs == constants.content_size_error) return .@"error";
            return .{ .known = fcs };
        },
        else => unreachable,
    }
}

pub fn compressedSize(src: []const u8) ZstdError!usize {
    if (src.len < 4) return error.SrcSizeWrong;
    const magic = std.mem.readInt(u32, src[0..4], .little);

    if (magic == constants.magic_number) {
        if (src.len < 5) return error.SrcSizeWrong;
        const descriptor = src[4];
        const fcs_flag = descriptor & 0x3;
        const dict_id_flag = (descriptor >> 3) & 0x3;
        const single_segment = (descriptor & 0x40) != 0;

        var header_size: usize = if (single_segment) 4 else 5;
        if (fcs_flag >= 1) header_size += @as(usize, 1) << @intCast(fcs_flag);
        if (dict_id_flag >= 1) {
            const field_size: usize = if (dict_id_flag == 3) 4 else @as(usize, 1) << @intCast(dict_id_flag - 1);
            header_size += field_size;
        }

        var pos = header_size;
        while (pos + 3 <= src.len) {
            const header_val: u32 = @as(u32, src[pos]) | (@as(u32, src[pos + 1]) << 8) | (@as(u32, src[pos + 2]) << 16);
            const is_last = (header_val & 1) != 0;
            const block_type: u2 = @truncate((header_val >> 1) & 0x3);
            const block_size: u32 = (header_val >> 3) + 1;
            pos += 3;

            if (block_type == 3) return error.CorruptionDetected;
            if (pos + block_size > src.len) return error.SrcSizeWrong;
            pos += block_size;

            if (is_last) {
                if ((descriptor & 0x4) != 0) pos += 4;
                return pos;
            }
        }
        return error.SrcSizeWrong;
    }

    if ((magic & constants.magic_skippable_mask) == constants.magic_skippable_start) {
        if (src.len < 8) return error.SrcSizeWrong;
        const frame_size = std.mem.readInt(u32, src[4..8], .little);
        return 8 + frame_size;
    }

    return error.PrefixUnknown;
}

pub fn dictId(src: []const u8) u32 {
    if (src.len < 5) return 0;
    const descriptor = src[4];
    const dict_id_flag = (descriptor >> 3) & 0x3;
    if (dict_id_flag == 0) return 0;

    const single_segment = (descriptor & 0x40) != 0;
    var base: usize = 5;
    if (!single_segment) base += 1;

    const field_size: usize = if (dict_id_flag == 3) 4 else @as(usize, 1) << @intCast(dict_id_flag - 1);
    if (base + field_size > src.len) return 0;
    if (field_size >= 4) return std.mem.readInt(u32, src[base..][0..4], .little);

    return 0;
}

test "isFrame" {
    const allocator = std.testing.allocator;
    const compress_mod = @import("compress.zig");
    const data = "Frame detection test";
    const compressed = try compress_mod.compress(allocator, data, .{});
    defer allocator.free(compressed);

    try std.testing.expect(isFrame(compressed));
    try std.testing.expect(!isFrame("not a frame"));
    try std.testing.expect(!isFrame(""));
    try std.testing.expect(!isFrame("ab"));
}

test "contentSize" {
    const allocator = std.testing.allocator;
    const compress_mod = @import("compress.zig");
    const original = "Content size detection test";
    const compressed = try compress_mod.compress(allocator, original, .{});
    defer allocator.free(compressed);

    const result = contentSize(compressed);
    switch (result) {
        .known => |size| try std.testing.expectEqual(@as(u64, original.len), size),
        else => try std.testing.expect(false),
    }
}

test "inspect" {
    const allocator = std.testing.allocator;
    const compress_mod = @import("compress.zig");
    const original = "Frame inspection test";
    const compressed = try compress_mod.compress(allocator, original, .{});
    defer allocator.free(compressed);

    const info = inspect(compressed);
    try std.testing.expect(info != null);
    const i = info.?;
    try std.testing.expectEqual(@as(u64, original.len), i.content_size.?);
    try std.testing.expect(i.single_segment);
}

test "empty input to inspect" {
    try std.testing.expectEqual(@as(?FrameInfo, null), inspect(""));
}

test "invalid magic to inspect" {
    try std.testing.expectEqual(@as(?FrameInfo, null), inspect("not zstd"));
}
