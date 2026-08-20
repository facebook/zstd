const std = @import("std");

pub const major: u32 = 1;
pub const minor: u32 = 6;
pub const release: u32 = 0;
pub const number: u32 = major * 10000 + minor * 100 + release;
pub const string: []const u8 = "1.6.0";

pub const clevel_default: i32 = 3;
pub const clevel_min: i32 = -131072;
pub const clevel_max: i32 = 22;

test "version number" {
    try std.testing.expectEqual(@as(u32, 10600), number);
}

test "version string" {
    try std.testing.expectEqualStrings("1.6.0", string);
}
