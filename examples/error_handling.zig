const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const original = "Error handling example for zstd.";
    const compressed = try zstd.compress(gpa, original, 3);
    defer gpa.free(compressed);

    std.debug.print("Version: {s}\n", .{zstd.version.versionString()});
    std.debug.print("Version number: {d}\n", .{zstd.version.versionNumber()});
    std.debug.print("Min compression level: {d}\n", .{zstd.version.minCLevel()});
    std.debug.print("Max compression level: {d}\n", .{zstd.version.maxCLevel()});
    std.debug.print("Default compression level: {d}\n", .{zstd.version.defaultCLevel()});

    const cs = zstd.getFrameContentSize(compressed);
    switch (cs) {
        .known => |size| std.debug.print("Content size: {d}\n", .{size}),
        .unknown => std.debug.print("Content size: unknown\n", .{}),
        .@"error" => std.debug.print("Content size: error\n", .{}),
    }

    var dctx = try zstd.Decompressor.init();
    defer dctx.deinit();

    const garbage = "this is not valid zstd data";
    const buf = try gpa.alloc(u8, 256);
    defer gpa.free(buf);

    const result = dctx.decompress(buf, garbage);
    if (result) |written| {
        std.debug.print("Unexpected success: {d} bytes\n", .{written});
    } else |err| {
        std.debug.print("Expected error: {any}\n", .{err});
    }

    std.debug.print("Error handling example complete\n", .{});
}
