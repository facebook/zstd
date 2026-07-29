const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    var cctx = try zstd.Compressor.init();
    defer cctx.deinit();
    try cctx.setParameter(.compression_level, 3);

    const original = "Decompressor parameter tuning example with explicit context.";
    const compressed = try cctx.compressAlloc(gpa, original);
    defer gpa.free(compressed);

    var dctx = try zstd.Decompressor.init();
    defer dctx.deinit();

    try dctx.setParameter(.window_log_max, 27);

    const bounds = try zstd.dctx.dParamGetBounds(.window_log_max);
    std.debug.print("window_log_max bounds: [{d}, {d}]\n", .{ bounds.lower_bound, bounds.upper_bound });

    const decompressed = try dctx.decompressAlloc(gpa, compressed, original.len);
    defer gpa.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);

    const mem = dctx.sizeof();
    std.debug.print("Decompressor memory usage: {d} bytes\n", .{mem});
    std.debug.print("Round trip OK\n", .{});
}
