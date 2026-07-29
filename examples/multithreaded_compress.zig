const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    var cctx = try zstd.Compressor.init();
    defer cctx.deinit();

    try cctx.setParameter(.nb_workers, 4);
    try cctx.setParameter(.compression_level, 6);

    const data = "Multithreaded compression example using nb_workers parameter for parallel compression across multiple threads.";

    const compressed = try cctx.compressAlloc(gpa, data);
    defer gpa.free(compressed);

    std.debug.print("Original:   {d} bytes\n", .{data.len});
    std.debug.print("Compressed: {d} bytes\n", .{compressed.len});

    var dctx = try zstd.Decompressor.init();
    defer dctx.deinit();

    const decompressed = try dctx.decompressAlloc(gpa, compressed, data.len);
    defer gpa.free(decompressed);

    try std.testing.expectEqualStrings(data, decompressed);
    std.debug.print("Round trip OK with multithreading\n", .{});
}
