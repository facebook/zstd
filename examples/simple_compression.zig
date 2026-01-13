const std = @import("std");
const zstd = @import("zstd");
const common = @import("common.zig");

pub fn main() !void {
    const allocator = std.heap.c_allocator;
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len != 2) {
        std.debug.print("wrong arguments\nusage:\n{s} FILE\n", .{args[0]});
        return;
    }

    const input_filename = args[1];
    const input_data = try common.readFile(allocator, input_filename);
    defer allocator.free(input_data);

    const dest_size = zstd.c.ZSTD_compressBound(input_data.len);
    const dest_buffer = try allocator.alloc(u8, dest_size);
    defer allocator.free(dest_buffer);

    const cSize = zstd.c.ZSTD_compress(dest_buffer.ptr, dest_size, input_data.ptr, input_data.len, 1);
    if (zstd.c.ZSTD_isError(cSize) != 0) {
        std.debug.print("error compressing: {s}\n", .{zstd.c.ZSTD_getErrorName(cSize)});
        return;
    }

    const out_filename = try common.createOutFilename(allocator, input_filename);
    defer allocator.free(out_filename);
    try common.writeFile(out_filename, dest_buffer[0..cSize]);

    std.debug.print("{s} : {d} -> {d} - {s}\n", .{
        input_filename,
        input_data.len,
        cSize,
        out_filename,
    });
}



