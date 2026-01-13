const std = @import("std");
const zstd = @import("zstd");
const common = @import("common.zig");

pub fn main() !void {
    const allocator = std.heap.c_allocator;
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 2) {
        std.debug.print("wrong arguments\nusage:\n{s} FILE(s)\n", .{args[0]});
        return;
    }

    // Pre-calculate max file size and max filename length
    var max_file_size: usize = 0;

    var i: usize = 1;
    while (i < args.len) : (i += 1) {
        const file = try std.fs.cwd().openFile(args[i], .{});
        const stat = try file.stat();
        file.close();

        if (stat.size > max_file_size) max_file_size = @intCast(stat.size);
    }

    const c_buffer_size = zstd.c.ZSTD_compressBound(max_file_size);
    const f_buffer = try allocator.alloc(u8, max_file_size);
    defer allocator.free(f_buffer);

    const c_buffer = try allocator.alloc(u8, c_buffer_size);
    defer allocator.free(c_buffer);

    const cctx = zstd.c.ZSTD_createCCtx();
    if (cctx == null) {
        std.debug.print("zstd.eateCCtx() failed!\n", .{});
        return;
    }
    defer _ = zstd.c.ZSTD_freeCCtx(cctx);

    i = 1;
    while (i < args.len) : (i += 1) {
        const input_filename = args[i];

        // Load file into shared buffer
        const file = try std.fs.cwd().openFile(input_filename, .{});
        const f_size = try file.readAll(f_buffer);
        file.close();

        // Compress
        const cSize = zstd.c.ZSTD_compressCCtx(cctx, c_buffer.ptr, c_buffer_size, f_buffer.ptr, f_size, 1);
        if (zstd.c.ZSTD_isError(cSize) != 0) {
            std.debug.print("error compressing: {s}\n", .{zstd.c.ZSTD_getErrorName(cSize)});
            continue;
        }

        const out_filename = try common.createOutFilename(allocator, input_filename);
        defer allocator.free(out_filename);
        try common.writeFile(out_filename, c_buffer[0..cSize]);

        std.debug.print("{s} : {d} -> {d} - {s} \n", .{
            input_filename,
            f_size,
            cSize,
            out_filename,
        });
    }

    std.debug.print("compressed {d} files \n", .{args.len - 1});
}



