const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("zstd.zig Dictionary Example\n", .{});
    std.debug.print("===========================\n\n", .{});

    // Sample data
    const sample1 = "This is the first sample text for dictionary compression. " ++
        "It contains common patterns and words.";
    const sample2 = "This is the second sample text with similar patterns. " ++
        "It shares common words with the first sample.";
    const sample3 = "The third sample text also shares patterns. " ++
        "Common words appear across all samples for effective compression.";

    // Create a simple dictionary manually
    // A real dictionary would be trained from many samples
    const dict_content = "common words: the, is, a, for, with, sample, text, patterns";

    // Pack samples into a single buffer
    const total_len = sample1.len + sample2.len + sample3.len;
    var samples_buf = try allocator.alloc(u8, total_len);
    defer allocator.free(samples_buf);

    @memcpy(samples_buf[0..sample1.len], sample1);
    @memcpy(samples_buf[sample1.len..][0..sample2.len], sample2);
    @memcpy(samples_buf[sample1.len + sample2.len ..][0..sample3.len], sample3);

    const sizes = [_]usize{ sample1.len, sample2.len, sample3.len };

    // Use finalizeDictionary to create a proper dictionary
    var dict_buf: [4096]u8 = undefined;
    const dict_size = try zstd.finalizeDictionary(
        &dict_buf,
        dict_buf.len,
        dict_content,
        samples_buf,
        &sizes,
        .{},
    );

    std.debug.print("Dictionary created: {d} bytes\n", .{dict_size});
    std.debug.print("Dictionary ID: {d}\n\n", .{zstd.getDictIDFromDict(dict_buf[0..dict_size])});

    // Create CDict and compress
    var cdict = zstd.CDict.init(dict_buf[0..dict_size], 3);
    defer cdict.deinit();

    std.debug.print("Compressing with CDict...\n", .{});
    const compressed = try cdict.compress(allocator, sample1);
    defer allocator.free(compressed);

    std.debug.print("Original size:     {d} bytes\n", .{sample1.len});
    std.debug.print("Compressed size:   {d} bytes\n", .{compressed.len});
    std.debug.print("Compression ratio: {d:.2}x\n\n", .{
        @as(f64, @floatFromInt(sample1.len)) / @as(f64, @floatFromInt(compressed.len)),
    });

    // Create DDict and decompress
    var ddict = zstd.DDict.init(dict_buf[0..dict_size]);
    defer ddict.deinit();

    std.debug.print("Decompressing with DDict...\n", .{});
    const decompressed = try ddict.decompress(allocator, compressed);
    defer allocator.free(decompressed);

    std.debug.print("Decompressed size: {d} bytes\n", .{decompressed.len});

    // Verify
    const match = std.mem.eql(u8, sample1, decompressed);
    std.debug.print("Content matches:   {}\n", .{match});

    if (match) {
        std.debug.print("\nDecompressed text:\n{s}\n", .{decompressed});
    }
}
