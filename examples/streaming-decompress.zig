const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("zstd.zig Streaming Decompression Example\n", .{});
    std.debug.print("========================================\n\n", .{});

    // First compress some data
    const original_text = "This is a test for streaming decompression. " ++
        "It demonstrates how to decompress data in chunks, " ++
        "which is useful for large files or streaming sources.";

    const compressed = try zstd.compress(allocator, original_text, .{});
    defer allocator.free(compressed);

    std.debug.print("Compressed: {d} bytes\n\n", .{compressed.len});

    // Initialize stream decompressor
    var sd = zstd.StreamDecompressor.init(allocator, .{});
    defer sd.deinit();

    // Demonstrate chunked decompression
    // Note: StreamDecompressor.decompressChunk processes complete frames,
    // so we simulate receiving the full compressed data at once
    std.debug.print("=== Decompression ===\n", .{});

    var d_buf: [4096]u8 = undefined;
    const result = try sd.decompressChunk(compressed, &d_buf);

    std.debug.print("Decompressed: {d} bytes\n", .{result.bytes_written});
    std.debug.print("Content matches: {}\n", .{std.mem.eql(u8, original_text, d_buf[0..result.bytes_written])});
    std.debug.print("\nDecompressed text:\n{s}\n", .{d_buf[0..result.bytes_written]});

    // Demonstrate multiple compress/decompress round trips
    std.debug.print("\n=== Multiple Round Trips ===\n", .{});

    const messages = [_][]const u8{
        "First message for round trip.",
        "Second message with different content.",
        "Third and final message.",
    };

    for (messages, 0..) |msg, i| {
        const c = try zstd.compress(allocator, msg, .{});
        defer allocator.free(c);

        var out: [4096]u8 = undefined;
        const r = try sd.decompressChunk(c, &out);

        std.debug.print("Message {d}: {s} -> {s} (match={})\n", .{
            i + 1,
            msg,
            out[0..r.bytes_written],
            std.mem.eql(u8, msg, out[0..r.bytes_written]),
        });
    }
}
