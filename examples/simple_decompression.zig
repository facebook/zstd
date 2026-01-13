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

    const content_size = zstd.c.ZSTD_getFrameContentSize(input_data.ptr, input_data.len);
    const ZSTD_CONTENTSIZE_ERROR: u64 = std.math.maxInt(u64) - 1;
    const ZSTD_CONTENTSIZE_UNKNOWN: u64 = std.math.maxInt(u64);

    if (content_size == ZSTD_CONTENTSIZE_ERROR) {
        std.debug.print("{s}: not compressed by zstd!\n", .{input_filename});
        return;
    }
    if (content_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        std.debug.print("{s}: original size unknown!\n", .{input_filename});
        return;
    }

    const dest_buffer = try allocator.alloc(u8, content_size);
    defer allocator.free(dest_buffer);

    const dSize = zstd.c.ZSTD_decompress(dest_buffer.ptr, content_size, input_data.ptr, input_data.len);
    if (zstd.c.ZSTD_isError(dSize) != 0) {
        std.debug.print("error decompressing: {s}\n", .{zstd.c.ZSTD_getErrorName(dSize)});
        return;
    }

    std.debug.print("{s} : {d} -> {d} \n", .{
        input_filename,
        input_data.len,
        dSize,
    });

    std.debug.print("Successfully decoded {s} (in memory)\n", .{input_filename});
}



