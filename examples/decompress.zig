const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("zstd.zig Decompression Example\n", .{});
    std.debug.print("=============================\n\n", .{});

    // Sample text to compress
    const original_text = "Hello, this is a test string for decompression! " ++
        "It contains repeated patterns like AAAA BBBB CCCC which compress well with zstd. " ++
        "The algorithm is based on LZ77, Huffman coding, and FSE.";

    // Compress first
    const compressed = try zstd.compress(allocator, original_text, .{});
    defer allocator.free(compressed);

    std.debug.print("Original size:     {d} bytes\n", .{original_text.len});
    std.debug.print("Compressed size:   {d} bytes\n", .{compressed.len});
    std.debug.print("Compression ratio: {d:.2}x\n\n", .{
        @as(f64, @floatFromInt(original_text.len)) / @as(f64, @floatFromInt(compressed.len)),
    });

    // Decompress
    const decompressed = try zstd.decompress(allocator, compressed, .{});
    defer allocator.free(decompressed);

    std.debug.print("Decompressed size: {d} bytes\n", .{decompressed.len});

    // Verify content matches
    const match = std.mem.eql(u8, original_text, decompressed);
    std.debug.print("Content matches:   {}\n", .{match});

    if (match) {
        std.debug.print("\nDecompressed text:\n{s}\n", .{decompressed});
    } else {
        std.debug.print("\nERROR: Decompressed content does not match!\n", .{});
    }
}
