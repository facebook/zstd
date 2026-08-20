const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("zstd.zig Compression Levels Example\n", .{});
    std.debug.print("===================================\n\n", .{});

    // Generate test data with patterns
    var data_buf: [10000]u8 = undefined;
    for (&data_buf, 0..) |*byte, i| {
        byte.* = @intCast((i % 256) ^ (i / 100));
    }

    // Named levels
    std.debug.print("=== Named Levels ===\n", .{});
    std.debug.print("{s:15} {s:>12} {s:>12}\n", .{ "Level", "Size", "Ratio" });
    std.debug.print("{s:15} {s:>12} {s:>12}\n", .{ "---", "---", "---" });

    const levels = [_]struct { name: []const u8, level: zstd.CLevel }{
        .{ .name = "fastest", .level = .fastest },
        .{ .name = "default", .level = .default },
        .{ .name = "best", .level = .best },
    };

    for (levels) |l| {
        const compressed = try zstd.compress(allocator, &data_buf, .{ .level = l.level });
        defer allocator.free(compressed);

        std.debug.print("{s:15} {d:>10} B {d:>10.2}x\n", .{
            l.name,
            compressed.len,
            @as(f64, @floatFromInt(data_buf.len)) / @as(f64, @floatFromInt(compressed.len)),
        });
    }

    // Raw numeric levels
    std.debug.print("\n=== Numeric Levels ===\n", .{});
    std.debug.print("{s:15} {s:>12} {s:>12}\n", .{ "Level", "Size", "Ratio" });
    std.debug.print("{s:15} {s:>12} {s:>12}\n", .{ "---", "---", "---" });

    const numeric_levels = [_]struct { name: []const u8, level: zstd.CLevel }{
        .{ .name = "level 1", .level = zstd.CLevel.fromInt(1) },
        .{ .name = "level 3", .level = zstd.CLevel.fromInt(3) },
        .{ .name = "level 6", .level = zstd.CLevel.fromInt(6) },
        .{ .name = "level 9", .level = zstd.CLevel.fromInt(9) },
        .{ .name = "level 12", .level = zstd.CLevel.fromInt(12) },
        .{ .name = "level 15", .level = zstd.CLevel.fromInt(15) },
    };

    for (numeric_levels) |l| {
        const compressed = try zstd.compress(allocator, &data_buf, .{ .level = l.level });
        defer allocator.free(compressed);

        std.debug.print("{s:15} {d:>10} B {d:>10.2}x\n", .{
            l.name,
            compressed.len,
            @as(f64, @floatFromInt(data_buf.len)) / @as(f64, @floatFromInt(compressed.len)),
        });
    }
}
