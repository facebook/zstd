const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const sample_count = 50;
    const sample_len = 128;

    var prng = std.Random.DefaultPrng.init(42);
    const random = prng.random();

    var samples_buf = try gpa.alloc(u8, sample_count * sample_len);
    defer gpa.free(samples_buf);

    var sample_sizes = try gpa.alloc(usize, sample_count);
    defer gpa.free(sample_sizes);

    for (0..sample_count) |i| {
        const start = i * sample_len;
        for (0..sample_len) |j| {
            samples_buf[start + j] = random.int(u8);
        }
        sample_sizes[i] = sample_len;
    }

    std.debug.print("Training dictionary from {d} samples ({d} bytes each)...\n", .{ sample_count, sample_len });

    const dict = zstd.zdict.trainFromSamples(gpa, samples_buf, sample_sizes, 4096) catch |err| {
        std.debug.print("Dictionary training failed: {any}\n", .{err});
        std.debug.print("(This can happen if data is not compressible enough)\n", .{});
        return;
    };
    defer gpa.free(dict);

    std.debug.print("Trained dictionary: {d} bytes\n", .{dict.len});

    const dict_id = zstd.zdict.getDictID(dict);
    std.debug.print("Dictionary ID: {d}\n", .{dict_id});

    const header_size = zstd.zdict.getDictHeaderSize(dict) catch 0;
    std.debug.print("Dictionary header size: {d} bytes\n", .{header_size});

    const test_data = "Test data for dictionary compression after training.";
    const compressed = try zstd.dict.compressUsingDict(gpa, test_data, dict, 3);
    defer gpa.free(compressed);

    const decompressed = try zstd.dict.decompressUsingDict(gpa, compressed, dict, test_data.len);
    defer gpa.free(decompressed);

    try std.testing.expectEqualStrings(test_data, decompressed);
    std.debug.print("Dictionary training and usage OK\n", .{});
}
