const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const original = "This is streaming compression example data that will be compressed chunk by chunk " ** 4;

    var scomp = try zstd.StreamingCompressor.init();
    defer scomp.deinit();

    try scomp.setParameter(.compression_level, 3);

    const in_buf_size = zstd.stream.cStreamInSize();
    const out_buf_size = zstd.stream.cStreamOutSize();
    std.debug.print("Recommended input buffer:  {d} bytes\n", .{in_buf_size});
    std.debug.print("Recommended output buffer: {d} bytes\n", .{out_buf_size});

    var compressed: std.ArrayList(u8) = .empty;
    defer compressed.deinit(gpa);

    var out_buf = try gpa.alloc(u8, out_buf_size);
    defer gpa.free(out_buf);

    const chunk_size = 64;
    var offset: usize = 0;
    while (offset < original.len) {
        const end = @min(offset + chunk_size, original.len);
        const chunk = original[offset..end];
        offset = end;

        const res = try scomp.compressChunk(chunk, out_buf, .@"continue");
        if (res.bytes_written > 0) {
            try compressed.appendSlice(gpa, out_buf[0..res.bytes_written]);
        }
    }

    const final_res = try scomp.endStream(out_buf);
    if (final_res.bytes_written > 0) {
        try compressed.appendSlice(gpa, out_buf[0..final_res.bytes_written]);
    }

    std.debug.print("Original:   {d} bytes\n", .{original.len});
    std.debug.print("Compressed: {d} bytes\n", .{compressed.items.len});

    var sdecomp = try zstd.StreamingDecompressor.init();
    defer sdecomp.deinit();

    var decomp_buf = try gpa.alloc(u8, original.len + 64);
    defer gpa.free(decomp_buf);

    const dres = try sdecomp.decompressChunk(compressed.items, decomp_buf);
    try std.testing.expect(dres.remaining == 0);

    try std.testing.expectEqualStrings(original, decomp_buf[0..original.len]);
    std.debug.print("Streaming round trip OK\n", .{});
}
