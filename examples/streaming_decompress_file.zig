const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const original = "Streaming decompression example demonstrating chunked decompression of zstd data.\n" ** 3;

    var scomp = try zstd.StreamingCompressor.init();
    defer scomp.deinit();

    const out_buf_size = zstd.stream.cStreamOutSize();
    var comp_out = try gpa.alloc(u8, out_buf_size);
    defer gpa.free(comp_out);

    var compressed: std.ArrayList(u8) = .empty;
    defer compressed.deinit(gpa);

    const res = try scomp.compressChunk(original, comp_out, .end);
    try compressed.appendSlice(gpa, comp_out[0..res.bytes_written]);

    std.debug.print("Compressed {d} -> {d} bytes\n", .{ original.len, compressed.items.len });

    var sdecomp = try zstd.StreamingDecompressor.init();
    defer sdecomp.deinit();

    const decomp_out_size = zstd.stream.dStreamOutSize();
    var decomp_out = try gpa.alloc(u8, decomp_out_size);
    defer gpa.free(decomp_out);

    var decompressed: std.ArrayList(u8) = .empty;
    defer decompressed.deinit(gpa);

    const chunk_size = 32;
    var offset: usize = 0;
    while (offset < compressed.items.len) {
        const end = @min(offset + chunk_size, compressed.items.len);
        const chunk = compressed.items[offset..end];
        offset = end;

        const dres = try sdecomp.decompressChunk(chunk, decomp_out);
        try decompressed.appendSlice(gpa, decomp_out[0..dres.bytes_written]);

        if (dres.remaining == 0 and offset == compressed.items.len) break;
    }

    try std.testing.expectEqualStrings(original, decompressed.items);
    std.debug.print("Streaming decompress round trip OK\n", .{});
}
