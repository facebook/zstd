const std = @import("std");
const zstd = @import("zstd");
const common = @import("common.zig");

pub fn main() !void {
    const allocator = std.heap.c_allocator;
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 3) {
        std.debug.print("wrong arguments\nusage:\n{s} [FILES] dictionary\n", .{args[0]});
        return;
    }

    const dict_filename = args[args.len - 1];
    std.debug.print("loading dictionary {s} \n", .{dict_filename});

    const dict_buffer = try common.readFile(allocator, dict_filename);
    defer allocator.free(dict_buffer);

    const cLevel = 3;
    const cdict = zstd.c.ZSTD_createCDict(dict_buffer.ptr, dict_buffer.len, cLevel);
    if (cdict == null) {
        std.debug.print("zstd.eateCDict() failed!\n", .{});
        return;
    }
    defer _ = zstd.c.ZSTD_freeCDict(cdict);

    const cctx = zstd.c.ZSTD_createCCtx();
    if (cctx == null) {
        std.debug.print("zstd.eateCCtx() failed!\n", .{});
        return;
    }
    defer _ = zstd.c.ZSTD_freeCCtx(cctx);

    var i: usize = 1;
    while (i < args.len - 1) : (i += 1) {
        const input_filename = args[i];
        const input_data = try common.readFile(allocator, input_filename);
        defer allocator.free(input_data);

        const dest_size = zstd.c.ZSTD_compressBound(input_data.len);
        const dest_buffer = try allocator.alloc(u8, dest_size);
        defer allocator.free(dest_buffer);

        const cSize = zstd.c.ZSTD_compress_usingCDict(cctx, dest_buffer.ptr, dest_size, input_data.ptr, input_data.len, cdict);
        if (zstd.c.ZSTD_isError(cSize) != 0) {
            std.debug.print("error compressing: {s}\n", .{zstd.c.ZSTD_getErrorName(cSize)});
            continue;
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

    std.debug.print("All {d} files compressed. \n", .{args.len - 2});
}



