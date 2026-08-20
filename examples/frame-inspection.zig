const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("zstd.zig Frame Inspection Example\n", .{});
    std.debug.print("=================================\n\n", .{});

    // Compress some data
    const text = "This text will be compressed into a zstd frame " ++
        "that we can then inspect for metadata.";

    const compressed = try zstd.compress(allocator, text, .{});
    defer allocator.free(compressed);

    std.debug.print("Compressed data: {d} bytes\n\n", .{compressed.len});

    // Check if it's a valid zstd frame
    const is_frame = zstd.Frame.isFrame(compressed);
    std.debug.print("Is valid zstd frame: {}\n\n", .{is_frame});

    if (!is_frame) {
        std.debug.print("Not a valid zstd frame, cannot inspect.\n", .{});
        return;
    }

    // Inspect frame details
    const info = zstd.Frame.inspect(compressed) orelse {
        std.debug.print("Failed to inspect frame.\n", .{});
        return;
    };

    std.debug.print("=== Frame Details ===\n", .{});
    std.debug.print("Window Size:        ", .{});
    if (info.window_size) |ws| {
        std.debug.print("{d} bytes\n", .{ws});
    } else {
        std.debug.print("unknown\n", .{});
    }
    std.debug.print("Dictionary ID:      ", .{});
    if (info.dictionary_id) |id| {
        std.debug.print("{d}\n", .{id});
    } else {
        std.debug.print("none\n", .{});
    }
    std.debug.print("Content Size:       ", .{});
    if (info.content_size) |cs| {
        std.debug.print("{d} bytes\n", .{cs});
    } else {
        std.debug.print("unknown\n", .{});
    }
    std.debug.print("Has Checksum:       {}\n", .{info.checksum});
    std.debug.print("Single Segment:     {}\n", .{info.single_segment});
    std.debug.print("FCS Flag:           {d}\n", .{info.fcs_flag});

    // Show hex dump of frame header (first 18 bytes max)
    std.debug.print("\n=== Frame Header (hex) ===\n", .{});
    const header_len = @min(compressed.len, 18);
    for (compressed[0..header_len], 0..) |byte, i| {
        if (i > 0 and i % 16 == 0) std.debug.print("\n", .{});
        std.debug.print("{X:0>2} ", .{byte});
    }
    std.debug.print("\n", .{});

    // Decompress to verify
    const decompressed = try zstd.decompress(allocator, compressed, .{});
    defer allocator.free(decompressed);

    std.debug.print("\nDecompressed text:\n{s}\n", .{decompressed});
}
