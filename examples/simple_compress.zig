const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const original = "Hello, zstd from Zig! This is a simple compress/decompress round trip example.";

    std.debug.print("Original ({d} bytes):\n  {s}\n\n", .{ original.len, original });

    const compressed = try zstd.compress(gpa, original, 3);
    defer gpa.free(compressed);

    std.debug.print("Compressed ({d} bytes):\n", .{compressed.len});

    const decompressed = try zstd.decompress(gpa, compressed, original.len);
    defer gpa.free(decompressed);

    std.debug.print("Decompressed ({d} bytes):\n  {s}\n\n", .{ decompressed.len, decompressed });

    try std.testing.expectEqualStrings(original, decompressed);
    std.debug.print("Round trip successful!\n", .{});
}
