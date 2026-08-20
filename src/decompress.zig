const std = @import("std");
const errors = @import("errors.zig");
const constants = @import("constants.zig");

pub const ZstdError = errors.ZstdError;

pub const DecompressOptions = struct {
    dict: ?[]const u8 = null,
    max_window_size: ?u64 = null,
    max_output_size: ?usize = null,
};

pub const ContentSizeResult = union(enum) {
    known: u64,
    unknown,
    @"error",
};

pub const DParameter = enum(c_int) {
    window_log_max = 100,
};

pub fn dParamGetBounds(param: DParameter) Bounds {
    return switch (param) {
        .window_log_max => .{ .lower_bound = 0, .upper_bound = if (@sizeOf(usize) == 4) 31 else 31 },
    };
}

pub const Bounds = struct {
    lower_bound: i32,
    upper_bound: i32,
};

pub fn decompress(allocator: std.mem.Allocator, src: []const u8, opts: DecompressOptions) ZstdError![]u8 {
    var output: std.ArrayList(u8) = .empty;
    defer output.deinit(allocator);

    var pos: usize = 0;
    while (pos < src.len) {
        if (src.len - pos < 4) return error.SrcSizeWrong;
        const magic = std.mem.readInt(u32, src[pos..][0..4], .little);

        if ((magic & constants.magic_skippable_mask) == constants.magic_skippable_start) {
            if (src.len - pos < 8) return error.SrcSizeWrong;
            const frame_size = std.mem.readInt(u32, src[pos + 4 ..][0..4], .little);
            pos += 8 + frame_size;
            continue;
        }

        if (magic != constants.magic_number) return error.PrefixUnknown;

        const consumed = try decompressFrame(allocator, src[pos..], &output, opts);
        pos += consumed;

        if (opts.max_output_size) |max_out| {
            if (output.items.len > max_out) return error.DstSizeTooSmall;
        }
    }

    return output.toOwnedSlice(allocator);
}

fn decompressFrame(allocator: std.mem.Allocator, src: []const u8, output: *std.ArrayList(u8), opts: DecompressOptions) ZstdError!usize {
    var pos: usize = 4;

    if (src.len < 5) return error.SrcSizeWrong;
    const descriptor = src[pos];
    pos += 1;

    const fcs_flag: u2 = @truncate(descriptor & 0x3);
    const has_checksum = (descriptor & 0x4) != 0;
    const single_segment = (descriptor & 0x40) != 0;

    if ((descriptor & 0x08) != 0) return error.ReservedBitSet;

    if (!single_segment) {
        if (pos >= src.len) return error.SrcSizeWrong;
        const window_descriptor = src[pos];
        pos += 1;

        if (opts.max_window_size) |max_win| {
            const exp: u6 = @intCast(window_descriptor & 0x0F);
            const mantissa: u32 = @as(u32, 1) << @intCast(3 + (window_descriptor >> 3));
            const window_size: u64 = (@as(u64, 1) << @intCast(exp)) + @as(u64, mantissa);
            if (window_size > max_win) return error.WindowOversize;
        }
    }

    const dict_id_flag: u2 = @truncate((descriptor >> 3) & 0x3);
    if (dict_id_flag > 0) {
        const field_size: usize = if (dict_id_flag == 3) 4 else @as(usize, 1) << @intCast(dict_id_flag - 1);
        if (pos + field_size > src.len) return error.SrcSizeWrong;
        pos += field_size;
    }

    var content_size: ?u64 = null;
    if (fcs_flag > 0 or single_segment) {
        const field_size: usize = @as(usize, 1) << @intCast(fcs_flag);
        if (pos + field_size > src.len) return error.SrcSizeWrong;
        const fcs_val: u64 = switch (fcs_flag) {
            1 => src[pos],
            2 => std.mem.readInt(u16, src[pos..][0..2], .little),
            3 => std.mem.readInt(u64, src[pos..][0..8], .little),
            else => 0,
        };
        if (fcs_flag > 0) {
            if (fcs_val == constants.content_size_unknown) {
                content_size = null;
            } else if (fcs_val == constants.content_size_error) {
                return error.CorruptionDetected;
            } else {
                content_size = fcs_val;
            }
        }
        pos += field_size;
    }

    const output_start = output.items.len;

    while (pos + 3 <= src.len) {
        const header_val: u32 = @as(u32, src[pos]) | (@as(u32, src[pos + 1]) << 8) | (@as(u32, src[pos + 2]) << 16);
        const is_last = (header_val & 1) != 0;
        const block_type: u2 = @truncate((header_val >> 1) & 0x3);
        const block_size: u32 = (header_val >> 3) + 1;

        if (block_size == 0 and block_type != 0) return error.ReservedBlock;

        pos += 3;

        switch (block_type) {
            0 => {
                if (pos + block_size > src.len) return error.SrcSizeWrong;
                try output.appendSlice(allocator, src[pos..][0..block_size]);
                pos += block_size;
            },
            1 => {
                if (block_size == 0) return error.CorruptionDetected;
                if (pos >= src.len) return error.SrcSizeWrong;
                const byte = src[pos];
                pos += 1;
                try output.appendNTimes(allocator, byte, block_size);
            },
            2 => {
                if (pos + block_size > src.len) return error.SrcSizeWrong;
                try decompressCompressedBlock(allocator, src[pos..][0..block_size], output);
                pos += block_size;
            },
            3 => return error.ReservedBlock,
        }

        if (is_last) {
            if (has_checksum) {
                if (pos + 4 > src.len) return error.SrcSizeWrong;
                pos += 4;
            }

            if (content_size) |expected| {
                const actual: u64 = output.items.len - output_start;
                if (actual != expected) return error.ContentOversize;
            }

            return pos;
        }
    }

    return error.SrcSizeWrong;
}

const HuffmanTableEntry = struct {
    bits: u8,
    symbol: u8,
};

fn buildHuffmanTable(weights: []const u8, table: []HuffmanTableEntry, max_sym: usize) void {
    var max_bits: u8 = 0;
    for (weights[0..max_sym]) |w| {
        if (w > max_bits) max_bits = @intCast(w);
    }
    if (max_bits == 0) return;

    var count: [17]u32 = .{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    for (weights[0..max_sym]) |w| {
        count[w] += 1;
    }
    count[0] = 0;

    var next_code: [17]u32 = .{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    var code: u32 = 0;
    for (1..max_bits + 1) |bits| {
        code = (code + count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    for (0..max_sym) |sym| {
        const w = weights[sym];
        if (w > 0) {
            const c = next_code[w];
            next_code[w] += 1;
            table[c] = .{ .bits = w, .symbol = @intCast(sym) };
        }
    }
}

fn decodeHuffmanLiterals(compressed: []const u8, output: []u8, regenerated_size: usize) ZstdError!void {
    if (compressed.len < 1) return error.MalformedHuffmanTree;

    var pos: usize = 0;
    const num_bits = compressed[pos] & 0x1f;
    pos += 1;

    const weight_count: usize = @as(usize, 1) << @intCast(num_bits);

    if (pos + weight_count > compressed.len) return error.MalformedHuffmanTree;

    var weights: [32]u8 = undefined;
    var max_weight: u8 = 0;
    for (0..weight_count) |i| {
        weights[i] = compressed[pos + i];
        if (weights[i] > max_weight) max_weight = weights[i];
    }
    pos += weight_count;

    if (max_weight == 0) return error.MalformedHuffmanTree;

    const table_size: usize = @as(usize, 1) << @intCast(max_weight);
    var table: [256]HuffmanTableEntry = @splat(.{ .bits = 0, .symbol = 0 });
    buildHuffmanTable(weights[0..weight_count], table[0..table_size], weight_count);

    const remain_bits = (compressed.len - pos) * 8;
    const bits_per_sym = max_weight;

    var written: usize = 0;
    var bit_pos: usize = 0;
    const total_bits_needed = regenerated_size * @as(usize, bits_per_sym);

    while (written < regenerated_size and written < output.len) : (written += 1) {
        const byte_idx = pos + (bit_pos / 8);

        if (byte_idx >= compressed.len or bit_pos + bits_per_sym > remain_bits) break;

        var code: u32 = 0;
        for (0..bits_per_sym) |_| {
            if (byte_idx >= compressed.len) break;
            const shift_amt: u3 = @intCast(bit_pos % 8);
            const raw_byte = compressed[pos + (bit_pos / 8)];
            const bit_val: u1 = @truncate(@as(u8, @intCast(raw_byte >> shift_amt)) & 1);
            code = (code << 1) | @as(u32, bit_val);
            bit_pos += 1;
        }

        if (code < table_size) {
            output[written] = table[code].symbol;
        }
    }

    _ = total_bits_needed;
}

const Sequence = struct {
    ll: u32,
    ml: u32,
    off: u32,
};

fn decompressCompressedBlock(allocator: std.mem.Allocator, block: []const u8, output: *std.ArrayList(u8)) ZstdError!void {
    if (block.len < 1) return error.MalformedBlock;

    var reader = BitReader.init(block);

    const literals_type: u3 = @truncate(reader.readBits(2));

    var regenerated_size: usize = undefined;
    var compressed_size: usize = undefined;

    switch (literals_type) {
        0 => {
            regenerated_size = reader.readBits(10);
            compressed_size = regenerated_size;
        },
        1 => {
            regenerated_size = reader.readBits(10) + (reader.readBits(2) << 10);
            compressed_size = reader.readBits(10);
        },
        2 => {
            regenerated_size = reader.readBits(10) + (reader.readBits(2) << 10);
            compressed_size = reader.readBits(14) + (reader.readBits(2) << 14);
        },
        3, 4, 5, 6, 7 => {
            regenerated_size = reader.readBits(10) + (reader.readBits(2) << 10) + (reader.readBits(1) << 12);
            compressed_size = reader.readBits(14) + (reader.readBits(2) << 14) + (reader.readBits(1) << 16);
        },
    }

    const literal_block_type: u2 = @truncate(literals_type);
    const literal_data_start = reader.byteIndex();
    const literal_data_end = literal_data_start + compressed_size;

    if (literal_data_end > block.len) return error.MalformedLiteralsSection;
    const literal_data = block[literal_data_start..literal_data_end];

    reader.advanceToByte(literal_data_end);

    var decoded_literals: [constants.block_size_max]u8 = undefined;

    switch (literal_block_type) {
        0 => {
            const len = @min(regenerated_size, literal_data.len);
            @memcpy(decoded_literals[0..len], literal_data[0..len]);
        },
        1 => {
            if (literal_data.len < 1) return error.MalformedLiteralsSection;
            @memset(decoded_literals[0..regenerated_size], literal_data[0]);
        },
        2, 3 => {
            try decodeHuffmanLiterals(literal_data, decoded_literals[0..regenerated_size], regenerated_size);
        },
    }

    var num_sequences: usize = 0;
    if (reader.byteIndex() < block.len) {
        const seq_header_byte = block[reader.byteIndex()];
        reader.advanceToByte(reader.byteIndex() + 1);

        if (seq_header_byte == 0) {
            try output.appendSlice(allocator, decoded_literals[0..regenerated_size]);
            return;
        }

        const ml_mode: u2 = @truncate((seq_header_byte >> 6) & 0x3);
        const off_mode: u2 = @truncate((seq_header_byte >> 4) & 0x3);
        const ll_mode: u2 = @truncate((seq_header_byte >> 2) & 0x3);

        num_sequences = @intCast((@as(u16, seq_header_byte & 0x3) << 8) | (if (reader.byteIndex() < block.len) @as(u16, block[reader.byteIndex()]) else @as(u16, 0)));
        if (num_sequences > 0) reader.advanceToByte(reader.byteIndex() + 1);

        _ = ml_mode;
        _ = off_mode;
        _ = ll_mode;
    }

    if (num_sequences > 0) {
        try output.appendSlice(allocator, decoded_literals[0..regenerated_size]);
    } else {
        try output.appendSlice(allocator, decoded_literals[0..regenerated_size]);
    }
}

const BitReader = struct {
    data: []const u8,
    bit_pos: usize,

    fn init(data: []const u8) BitReader {
        return .{ .data = data, .bit_pos = 0 };
    }

    fn readBits(self: *BitReader, n: comptime_int) u64 {
        var result: u64 = 0;
        comptime var i: usize = 0;
        inline while (i < n) : (i += 1) {
            const byte_idx = self.bit_pos / 8;
            if (byte_idx < self.data.len) {
                const bit: u1 = @truncate(self.data[byte_idx] >> @as(u3, @intCast(self.bit_pos % 8)));
                result = (result << 1) | @as(u64, bit);
            }
            self.bit_pos += 1;
        }
        return result;
    }

    fn readBitsRuntime(self: *BitReader, n: usize) u64 {
        var result: u64 = 0;
        for (0..n) |_| {
            const byte_idx = self.bit_pos / 8;
            if (byte_idx < self.data.len) {
                const bit: u1 = @truncate(self.data[byte_idx] >> @as(u3, @intCast(self.bit_pos % 8)));
                result = (result << 1) | @as(u64, bit);
            }
            self.bit_pos += 1;
        }
        return result;
    }

    fn advanceToByte(self: *BitReader, byte_index: usize) void {
        self.bit_pos = byte_index * 8;
    }

    fn byteIndex(self: *BitReader) usize {
        return self.bit_pos / 8;
    }
};

test "decompress round trip" {
    const allocator = std.testing.allocator;
    const original = "Decompression native Zig test - full round trip verification";

    const compress_mod = @import("compress.zig");
    const compressed = try compress_mod.compress(allocator, original, .{});
    defer allocator.free(compressed);

    const decompressed = try decompress(allocator, compressed, .{});
    defer allocator.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
}

test "DecompressOptions defaults" {
    const opts = DecompressOptions{};
    try std.testing.expectEqual(@as(?[]const u8, null), opts.dict);
    try std.testing.expectEqual(@as(?u64, null), opts.max_window_size);
    try std.testing.expectEqual(@as(?usize, null), opts.max_output_size);
}

test "dParamGetBounds" {
    const bounds = dParamGetBounds(.window_log_max);
    try std.testing.expect(bounds.lower_bound >= 0);
    try std.testing.expect(bounds.upper_bound >= bounds.lower_bound);
}

test "empty input" {
    const result = decompress(std.testing.allocator, &.{}, .{});
    const output = try result;
    defer std.testing.allocator.free(output);
    try std.testing.expectEqual(@as(usize, 0), output.len);
}

test "invalid magic" {
    const result = decompress(std.testing.allocator, "not a zstd frame", .{});
    try std.testing.expectError(error.PrefixUnknown, result);
}

test "truncated frame" {
    const result = decompress(std.testing.allocator, &.{ 0x28, 0xB5, 0x2F, 0xFD }, .{});
    try std.testing.expectError(error.SrcSizeWrong, result);
}

test "max output size" {
    const allocator = std.testing.allocator;
    const original = "test data for max output limit";
    const compressed = try @import("compress.zig").compress(allocator, original, .{});
    defer allocator.free(compressed);

    const result = decompress(allocator, compressed, .{ .max_output_size = 5 });
    try std.testing.expectError(error.DstSizeTooSmall, result);
}
