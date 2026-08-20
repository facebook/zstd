const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("zstd.zig Basic Example\n", .{});
    std.debug.print("======================\n\n", .{});

    // Sample text
    const text = "Hello, zstd.zig! This is a test of the pure Zig compression library.";

    // Compress
    const compressed = try zstd.compress(allocator, text, .{});
    defer allocator.free(compressed);

    std.debug.print("Original:     {d} bytes\n", .{text.len});
    std.debug.print("Compressed:   {d} bytes\n", .{compressed.len});
    std.debug.print("Ratio:        {d:.2}x\n\n", .{
        @as(f64, @floatFromInt(text.len)) / @as(f64, @floatFromInt(compressed.len)),
    });

    // Decompress
    const decompressed = try zstd.decompress(allocator, compressed, .{});
    defer allocator.free(decompressed);

    std.debug.print("Decompressed: {d} bytes\n", .{decompressed.len});
    std.debug.print("Match:        {}\n\n", .{std.mem.eql(u8, text, decompressed)});
    std.debug.print("Text: {s}\n", .{decompressed});
}
