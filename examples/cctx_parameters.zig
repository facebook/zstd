const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    var cctx = try zstd.Compressor.init();
    defer cctx.deinit();

    try cctx.setParameter(.compression_level, 1);
    try cctx.setParameter(.window_log, 14);
    try cctx.setParameter(.checksum_flag, 1);

    const bounds = try zstd.cctx.cParamGetBounds(.compression_level);
    std.debug.print("compression_level bounds: [{d}, {d}]\n", .{ bounds.lower_bound, bounds.upper_bound });

    const original = "Tuning compression parameters with an explicit Compressor context for optimal results.";
    const compressed = try cctx.compressAlloc(gpa, original);
    defer gpa.free(compressed);

    std.debug.print("Original:      {d} bytes\n", .{original.len});
    std.debug.print("Compressed:    {d} bytes\n", .{compressed.len});

    var dctx = try zstd.Decompressor.init();
    defer dctx.deinit();

    const decompressed = try dctx.decompressAlloc(gpa, compressed, original.len);
    defer gpa.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
    std.debug.print("Round trip OK\n", .{});
}
