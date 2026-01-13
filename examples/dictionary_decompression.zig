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

    const ddict = zstd.c.ZSTD_createDDict(dict_buffer.ptr, dict_buffer.len);
    if (ddict == null) {
        std.debug.print("zstd.eateDDict() failed!\n", .{});
        return;
    }
    defer _ = zstd.c.ZSTD_freeDDict(ddict);

    const dctx = zstd.c.ZSTD_createDCtx();
    if (dctx == null) {
        std.debug.print("zstd.eateDCtx() failed!\n", .{});
        return;
    }
    defer _ = zstd.c.ZSTD_freeDCtx(dctx);

    // Manual definitions to avoid translate-c overflow issues
    const ZSTD_CONTENTSIZE_ERROR: u64 = std.math.maxInt(u64) - 1;
    const ZSTD_CONTENTSIZE_UNKNOWN: u64 = std.math.maxInt(u64);

    var i: usize = 1;
    while (i < args.len - 1) : (i += 1) {
        const input_filename = args[i];
        const input_data = try common.readFile(allocator, input_filename);
        defer allocator.free(input_data);

        const content_size = zstd.c.ZSTD_getFrameContentSize(input_data.ptr, input_data.len);
        if (content_size == ZSTD_CONTENTSIZE_ERROR) {
            std.debug.print("{s}: not compressed by zstd!\n", .{input_filename});
            continue;
        }
        if (content_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            std.debug.print("{s}: original size unknown!\n", .{input_filename});
            continue;
        }

        const dest_buffer = try allocator.alloc(u8, content_size);
        defer allocator.free(dest_buffer);

        const expectedDictID = zstd.c.ZSTD_getDictID_fromDDict(ddict);
        const actualDictID = zstd.c.ZSTD_getDictID_fromFrame(input_data.ptr, input_data.len);
        if (actualDictID != expectedDictID) {
            std.debug.print("DictID mismatch: expected {d} got {d}\n", .{ expectedDictID, actualDictID });
            continue;
        }

        const dSize = zstd.c.ZSTD_decompress_usingDDict(dctx, dest_buffer.ptr, content_size, input_data.ptr, input_data.len, ddict);
        if (zstd.c.ZSTD_isError(dSize) != 0) {
            std.debug.print("error decompressing: {s}\n", .{zstd.c.ZSTD_getErrorName(dSize)});
            continue;
        }

        std.debug.print("{s} : {d} -> {d} \n", .{
            input_filename,
            input_data.len,
            dSize,
        });
    }

    std.debug.print("All {d} files correctly decoded (in memory) \n", .{args.len - 2});
}



