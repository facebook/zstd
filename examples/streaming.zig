const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("zstd.zig Streaming Example\n", .{});
    std.debug.print("==========================\n\n", .{});

    // Initialize stream compressor
    var sc = zstd.StreamCompressor.init(allocator, .{});
    defer sc.deinit();

    // Simulate streaming data
    const chunks = [_][]const u8{
        "First chunk of data. ",
        "Second chunk with more content. ",
        "Third and final chunk. ",
    };

    std.debug.print("=== Compressing {d} chunks ===\n", .{chunks.len});

    for (chunks, 0..) |chunk, i| {
        var buf: [4096]u8 = undefined;
        const result = try sc.compressChunk(chunk, &buf, .end);
        std.debug.print("Chunk {d}: {d} bytes -> {d} bytes\n", .{ i + 1, chunk.len, result.bytes_written });
    }

    // Finish
    var final_buf: [4096]u8 = undefined;
    const final_result = try sc.endStream(&final_buf);

    std.debug.print("Final: {d} bytes\n\n", .{final_result.bytes_written});

    // Stream decompress
    std.debug.print("=== Stream Decompression ===\n", .{});

    var sd = zstd.StreamDecompressor.init(allocator, .{});
    defer sd.deinit();

    // Re-compress for decompression test
    const full_compressed = try zstd.compress(allocator, "Stream decompression test.", .{});
    defer allocator.free(full_compressed);

    var d_buf: [4096]u8 = undefined;
    const dresult = try sd.decompressChunk(full_compressed, &d_buf);

    std.debug.print("Decompressed: {d} bytes\n", .{dresult.bytes_written});
    std.debug.print("Text: {s}\n", .{d_buf[0..dresult.bytes_written]});
}
