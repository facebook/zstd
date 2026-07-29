const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const dict_content = "Dictionary decompression example with prepared DDict for efficient repeated decompression.";

    var cdict = try zstd.CDict.init(dict_content, 3);
    defer cdict.deinit();

    const original = "This data was compressed with a dictionary and will be decompressed using a prepared DDict.";

    const compressed = try zstd.dict.compressUsingCDict(gpa, original, &cdict);
    defer gpa.free(compressed);

    var ddict = try zstd.DDict.init(dict_content);
    defer ddict.deinit();

    std.debug.print("DDict created, dict_id = {d}\n", .{ddict.getDictId()});

    const decompressed = try zstd.dict.decompressUsingDDict(gpa, compressed, &ddict, original.len);
    defer gpa.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);

    const frame_dict_id = zstd.dict.getDictIDFromFrame(compressed);
    std.debug.print("Frame dict_id: {d}\n", .{frame_dict_id});

    const raw_dict_id = zstd.dict.getDictIDFromDict(dict_content);
    std.debug.print("Raw dict_id:   {d}\n", .{raw_dict_id});

    std.debug.print("Dictionary decompress round trip OK\n", .{});
}
