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

    const fin = try std.fs.cwd().openFile(input_filename, .{});
    defer fin.close();

    // Write to stdout (std.io.getStdOut() is missing in 0.15?, using debug print as fallback)
    // const stdout = std.io.getStdOut().writer();

    const buffInSize = zstd.c.ZSTD_DStreamInSize();
    const buffIn = try allocator.alloc(u8, buffInSize);
    defer allocator.free(buffIn);

    const buffOutSize = zstd.c.ZSTD_DStreamOutSize();
    const buffOut = try allocator.alloc(u8, buffOutSize);
    defer allocator.free(buffOut);

    const dctx = zstd.c.ZSTD_createDCtx();
    if (dctx == null) return error.ZstdCreateDCtxFailed;
    defer _ = zstd.c.ZSTD_freeDCtx(dctx);

    // var r = fin.reader();

    while (true) {
        const read = try fin.read(buffIn);
        if (read == 0) break;

        var input = zstd.c.ZSTD_inBuffer{
            .src = buffIn.ptr,
            .size = read,
            .pos = 0,
        };

        while (input.pos < input.size) {
            var output = zstd.c.ZSTD_outBuffer{
                .dst = buffOut.ptr,
                .size = buffOutSize,
                .pos = 0,
            };

            const ret = zstd.c.ZSTD_decompressStream(dctx, &output, &input);
            if (zstd.c.ZSTD_isError(ret) != 0) {
                std.debug.print("Decompression error: {s}\n", .{zstd.c.ZSTD_getErrorName(ret)});
                return error.DecompressionFailed;
            }

            // try stdout.writeAll(buffOut[0..output.pos]);
            std.debug.print("{s}", .{buffOut[0..output.pos]});
        }
    }
}



