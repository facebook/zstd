const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const original = "This example demonstrates using compressBound to pre-size the compression buffer.";

    const bound = try zstd.compressBound(original.len);
    std.debug.print("Input size:      {d} bytes\n", .{original.len});
    std.debug.print("compressBound:   {d} bytes\n", .{bound});

    const dst = try gpa.alloc(u8, bound);
    defer gpa.free(dst);

    const cctx = try zstd.Compressor.init();
    defer cctx.deinit();

    const written = try cctx.compress2(dst, original);
    std.debug.print("Actual compressed: {d} bytes\n\n", .{written});

    std.debug.print("Compression ratio: {d:.2}x\n", .{
        @as(f64, @floatFromInt(original.len)) / @as(f64, @floatFromInt(written)),
    });
}
