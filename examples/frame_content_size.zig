const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const original = "Inspecting frame content size stored in the zstd frame header.";

    const compressed = try zstd.compress(gpa, original, 3);
    defer gpa.free(compressed);

    const cs = zstd.getFrameContentSize(compressed);
    switch (cs) {
        .known => |size| {
            std.debug.print("Frame content size: {d} bytes\n", .{size});
            try std.testing.expectEqual(@as(u64, original.len), size);
        },
        .unknown => std.debug.print("Frame content size: unknown\n", .{}),
        .@"error" => std.debug.print("Frame content size: error (invalid frame)\n", .{}),
    }

    const comp_size = try zstd.findFrameCompressedSize(compressed);
    std.debug.print("Frame compressed size: {d} bytes\n", .{comp_size});

    std.debug.print("Is valid zstd frame: {}\n", .{zstd.isFrame(compressed)});
    std.debug.print("Is valid zstd frame (garbage): {}\n", .{zstd.isFrame("not a frame")});
}
