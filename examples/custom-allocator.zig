const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    std.debug.print("zstd.zig Custom Allocator Example\n", .{});
    std.debug.print("==================================\n\n", .{});

    // Use a page allocator
    const page_allocator = std.heap.page_allocator;

    // Sample data
    const text = "Custom allocator example using page allocator.";

    // Compress with page allocator
    const compressed = try zstd.compress(page_allocator, text, .{});
    defer page_allocator.free(compressed);

    std.debug.print("Compressed with page allocator: {d} bytes\n", .{compressed.len});

    // Decompress with page allocator
    const decompressed = try zstd.decompress(page_allocator, compressed, .{});
    defer page_allocator.free(decompressed);

    std.debug.print("Decompressed with page allocator: {d} bytes\n", .{decompressed.len});

    // Verify
    const match = std.mem.eql(u8, text, decompressed);
    std.debug.print("Content matches: {}\n", .{match});

    // Arena allocator example
    std.debug.print("\n=== Arena Allocator ===\n", .{});

    var arena = std.heap.ArenaAllocator.init(page_allocator);
    defer arena.deinit();

    const arena_allocator = arena.allocator();

    // Streaming with arena
    var sc = zstd.StreamCompressor.init(arena_allocator, .{});
    defer sc.deinit();

    var buf: [4096]u8 = undefined;
    const chunk1 = "First chunk of data ";
    const r1 = try sc.compressChunk(chunk1, &buf, .@"continue");

    const chunk2 = "for arena streaming.";
    const r2 = try sc.compressChunk(chunk2, &buf, .@"continue");

    const final = try sc.endStream(&buf);

    std.debug.print("Streamed with arena: {d} bytes\n", .{r1.bytes_written + r2.bytes_written + final.bytes_written});
}
