const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("zstd.zig Advanced Parameters Example\n", .{});
    std.debug.print("=====================================\n\n", .{});

    // Sample data
    const text = "Advanced parameters example with custom window and block sizes.";

    // Query parameter bounds using cParamGetBounds
    std.debug.print("=== Parameter Bounds ===\n", .{});

    const window_log = try zstd.cParamGetBounds(.window_log);
    std.debug.print("Window Log:  min={d}, max={d}\n", .{ window_log.lower_bound, window_log.upper_bound });

    const hash_log = try zstd.cParamGetBounds(.hash_log);
    std.debug.print("Hash Log:    min={d}, max={d}\n", .{ hash_log.lower_bound, hash_log.upper_bound });

    const chain_log = try zstd.cParamGetBounds(.chain_log);
    std.debug.print("Chain Log:   min={d}, max={d}\n", .{ chain_log.lower_bound, chain_log.upper_bound });

    const search_log = try zstd.cParamGetBounds(.search_log);
    std.debug.print("Search Log:  min={d}, max={d}\n", .{ search_log.lower_bound, search_log.upper_bound });

    const min_match = try zstd.cParamGetBounds(.min_match);
    std.debug.print("Min Match:   min={d}, max={d}\n", .{ min_match.lower_bound, min_match.upper_bound });

    // Compress with custom parameters
    std.debug.print("\n=== Custom Parameters ===\n", .{});

    const compressed = try zstd.compress(allocator, text, .{
        .level = zstd.CLevel.fromInt(6),
        .window_log = 14,
        .strategy = .greedy,
    });
    defer allocator.free(compressed);

    std.debug.print("Compressed with custom params: {d} bytes\n", .{compressed.len});

    // Verify decompression
    const decompressed = try zstd.decompress(allocator, compressed, .{});
    defer allocator.free(decompressed);

    const match = std.mem.eql(u8, text, decompressed);
    std.debug.print("Decompression verified: {}\n", .{match});

    // Strategy comparison
    std.debug.print("\n=== Strategy Comparison ===\n", .{});
    std.debug.print("{s:15} {s:>12}\n", .{ "Strategy", "Size" });
    std.debug.print("{s:15} {s:>12}\n", .{ "---", "---" });

    const strategies = [_]struct { name: []const u8, strategy: zstd.Strategy }{
        .{ .name = "fast", .strategy = .fast },
        .{ .name = "dfast", .strategy = .dfast },
        .{ .name = "greedy", .strategy = .greedy },
        .{ .name = "lazy", .strategy = .lazy },
        .{ .name = "lazy2", .strategy = .lazy2 },
        .{ .name = "btlazy2", .strategy = .btlazy2 },
        .{ .name = "btopt", .strategy = .btopt },
        .{ .name = "btultra", .strategy = .btultra },
    };

    for (strategies) |s| {
        const c = try zstd.compress(allocator, text, .{ .strategy = s.strategy });
        defer allocator.free(c);

        std.debug.print("{s:15} {d:>10} B\n", .{ s.name, c.len });
    }
}
