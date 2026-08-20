const std = @import("std");

pub const magic_number: u32 = 0xFD2FB528;
pub const magic_dictionary: u32 = 0xEC30A437;
pub const magic_skippable_start: u32 = 0x184D2A50;
pub const magic_skippable_mask: u32 = 0xFFFFFFF0;
pub const block_size_log_max: u32 = 17;
pub const block_size_max: u32 = 1 << block_size_log_max;
pub const content_size_unknown: u64 = 0xFFFFFFFFFFFFFFFF;
pub const content_size_error: u64 = 0xFFFFFFFFFFFFFFFE;
pub const max_input_size: u64 = if (@sizeOf(usize) == 8) 0xFF00FF00FF00FF00 else 0xFF00FF00;

test "magic number" {
    try std.testing.expectEqual(@as(u32, 0xFD2FB528), magic_number);
}

test "block size max" {
    try std.testing.expectEqual(@as(u32, 131072), block_size_max);
}
