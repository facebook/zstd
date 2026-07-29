const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const original = "Benchmarking compression levels to compare speed and ratio trade-offs across different zstd settings. " ** 5;

    const min_level = zstd.version.minCLevel();
    const max_level = zstd.version.maxCLevel();
    const default_level = zstd.version.defaultCLevel();

    std.debug.print("Compression level range: [{d}, {d}], default: {d}\n\n", .{ min_level, max_level, default_level });

    std.debug.print("Level | Compressed | Ratio\n", .{});
    std.debug.print("------+------------+-------\n", .{});

    const levels = [_]i32{ 1, 2, 3, 5, 8, 12, 16, 19 };

    inline for (levels) |level| {
        const compressed = try zstd.compress(gpa, original, level);
        defer gpa.free(compressed);

        const ratio = @as(f64, @floatFromInt(original.len)) / @as(f64, @floatFromInt(compressed.len));

        std.debug.print("{d: >5} | {d: >10} | {d: >5.2}x\n", .{
            level,
            compressed.len,
            ratio,
        });
    }

    std.debug.print("\nBenchmark complete\n", .{});
}
