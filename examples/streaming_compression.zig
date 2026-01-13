const std = @import("std");
const zstd = @import("zstd");
const common = @import("common.zig");

pub fn main() !void {
    const allocator = std.heap.c_allocator;
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 2) {
        std.debug.print("wrong arguments\nusage:\n{s} FILE [LEVEL] [THREADS]\n", .{args[0]});
        return;
    }

    const input_filename = args[1];
    var cLevel: c_int = 1;
    if (args.len >= 3) {
        cLevel = try std.fmt.parseInt(c_int, args[2], 10);
    }
    var nbThreads: c_int = 1;
    if (args.len >= 4) {
        nbThreads = try std.fmt.parseInt(c_int, args[3], 10);
    }

    const out_filename = try common.createOutFilename(allocator, input_filename);
    defer allocator.free(out_filename);

    std.debug.print("Starting compression of {s} with level {d}, using {d} threads\n", .{ input_filename, cLevel, nbThreads });

    const fin = try std.fs.cwd().openFile(input_filename, .{});
    defer fin.close();

    const fout = try std.fs.cwd().createFile(out_filename, .{});
    defer fout.close();

    const buffInSize = zstd.c.ZSTD_CStreamInSize();
    const buffIn = try allocator.alloc(u8, buffInSize);
    defer allocator.free(buffIn);

    const buffOutSize = zstd.c.ZSTD_CStreamOutSize();
    const buffOut = try allocator.alloc(u8, buffOutSize);
    defer allocator.free(buffOut);

    const cctx = zstd.c.ZSTD_createCCtx();
    if (cctx == null) return error.ZstdCreateCCtxFailed;
    defer _ = zstd.c.ZSTD_freeCCtx(cctx);

    _ = zstd.c.ZSTD_CCtx_setParameter(cctx, zstd.c.ZSTD_c_compressionLevel, cLevel);
    _ = zstd.c.ZSTD_CCtx_setParameter(cctx, zstd.c.ZSTD_c_checksumFlag, 1);
    if (nbThreads > 1) {
        _ = zstd.c.ZSTD_CCtx_setParameter(cctx, zstd.c.ZSTD_c_nbWorkers, nbThreads);
    }

    // var r = fin.reader();
    // var w = fout.writer();

    while (true) {
        const read = try fin.read(buffIn);

        const lastChunk = (read < buffInSize);
        const mode: zstd.c.ZSTD_EndDirective = if (lastChunk) zstd.c.ZSTD_e_end else zstd.c.ZSTD_e_continue;

        var input = zstd.c.ZSTD_inBuffer{
            .src = buffIn.ptr,
            .size = read,
            .pos = 0,
        };

        var finished = false;
        while (!finished) {
            var output = zstd.c.ZSTD_outBuffer{
                .dst = buffOut.ptr,
                .size = buffOutSize,
                .pos = 0,
            };

            const remaining = zstd.c.ZSTD_compressStream2(cctx, &output, &input, mode);
            if (zstd.c.ZSTD_isError(remaining) != 0) {
                std.debug.print("Compression error: {s}\n", .{zstd.c.ZSTD_getErrorName(remaining)});
                return error.CompressionFailed;
            }

            try fout.writeAll(buffOut[0..output.pos]);

            finished = if (lastChunk) (remaining == 0) else (input.pos == input.size);
        }

        if (lastChunk) break;
    }
}



