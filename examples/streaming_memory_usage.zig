const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const allocator = std.heap.c_allocator;
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    std.debug.print("\n Zstandard (v{s}) memory usage for streaming : \n\n", .{zstd.c.ZSTD_versionString()});

    var wLog: c_int = 0;
    if (args.len > 1) {
        wLog = try std.fmt.parseInt(c_int, args[1], 10);
    }

    // Hard to replicate full logic of readU32FromChar fully without more code, simplified to int parse
    // Original C code handles suffixes like K, M, etc. For simplicity here assume bytes or basic int.

    var compressionLevel: c_int = 1;
    const MAX_TESTED_LEVEL = 12;

    while (compressionLevel <= MAX_TESTED_LEVEL) : (compressionLevel += 1) {
        const dataToCompress = "abcde";
        var compressedData: [128]u8 = undefined;
        var decompressedData: [5]u8 = undefined;

        const cctxParams = zstd.c.ZSTD_createCCtxParams();
        defer _ = zstd.c.ZSTD_freeCCtxParams(cctxParams);

        _ = zstd.c.ZSTD_CCtxParams_setParameter(cctxParams, zstd.c.ZSTD_c_compressionLevel, compressionLevel);
        _ = zstd.c.ZSTD_CCtxParams_setParameter(cctxParams, zstd.c.ZSTD_c_windowLog, wLog);

        const cctx = zstd.c.ZSTD_createCCtx();
        defer _ = zstd.c.ZSTD_freeCCtx(cctx);

        _ = zstd.c.ZSTD_CCtx_setParametersUsingCCtxParams(cctx, cctxParams);

        var compressedSize: usize = 0;
        {
            var inBuff = zstd.c.ZSTD_inBuffer{ .src = dataToCompress, .size = dataToCompress.len, .pos = 0 };
            var outBuff = zstd.c.ZSTD_outBuffer{ .dst = &compressedData, .size = compressedData.len, .pos = 0 };

            _ = zstd.c.ZSTD_compressStream(cctx, &outBuff, &inBuff);
            const remaining = zstd.c.ZSTD_endStream(cctx, &outBuff);

            if (remaining != 0) {
                std.debug.print("Frame not flushed!\n", .{});
                return;
            }
            compressedSize = outBuff.pos;
        }

        const dctx = zstd.c.ZSTD_createDCtx();
        defer _ = zstd.c.ZSTD_freeDCtx(dctx);

        _ = zstd.c.ZSTD_DCtx_setParameter(dctx, zstd.c.ZSTD_d_windowLogMax, wLog);

        {
            var inBuff = zstd.c.ZSTD_inBuffer{ .src = &compressedData, .size = compressedSize, .pos = 0 };
            var outBuff = zstd.c.ZSTD_outBuffer{ .dst = &decompressedData, .size = decompressedData.len, .pos = 0 };

            const remaining = zstd.c.ZSTD_decompressStream(dctx, &outBuff, &inBuff);
            if (remaining != 0) {
                std.debug.print("Frame not complete!\n", .{});
                return;
            }
        }

        const cstreamSize = zstd.c.ZSTD_sizeof_CStream(cctx);
        const cstreamEstimatedSize = zstd.c.ZSTD_estimateCStreamSize_usingCCtxParams(cctxParams);
        const dstreamSize = zstd.c.ZSTD_sizeof_DStream(dctx);
        const dstreamEstimatedSize = zstd.c.ZSTD_estimateDStreamSize_fromFrame(compressedData[0..compressedSize].ptr, compressedSize);

        if (cstreamSize > cstreamEstimatedSize) std.debug.print("Compression mem warning\n", .{});
        if (dstreamSize > dstreamEstimatedSize) std.debug.print("Decompression mem warning\n", .{});

        std.debug.print("Level {d:2} : Compression Mem = {d:5} KB (estimated : {d:5} KB) ; Decompression Mem = {d:4} KB (estimated : {d:5} KB)\n", .{ compressionLevel, cstreamSize >> 10, cstreamEstimatedSize >> 10, dstreamSize >> 10, dstreamEstimatedSize >> 10 });

        if (wLog != 0) break;
    }
}




