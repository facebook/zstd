const zstd = @import("zstd");
const std = @import("std");

pub fn main() !void {
    const gpa = std.heap.page_allocator;

    const dict_content = "This dictionary content is used for dictionary-based compression to improve ratio on small data.";

    var cdict = try zstd.CDict.init(dict_content, 3);
    defer cdict.deinit();

    std.debug.print("CDict created, dict_id = {d}\n", .{cdict.getDictId()});

    const original = "Dictionary compression example using a prepared CDict for repeated use.";

    const compressed = try zstd.dict.compressUsingCDict(gpa, original, &cdict);
    defer gpa.free(compressed);

    std.debug.print("Original:      {d} bytes\n", .{original.len});
    std.debug.print("With dict:     {d} bytes\n", .{compressed.len});

    const no_dict = try zstd.compress(gpa, original, 3);
    defer gpa.free(no_dict);
    std.debug.print("Without dict:  {d} bytes\n", .{no_dict.len});

    var ddict = try zstd.DDict.init(dict_content);
    defer ddict.deinit();

    const decompressed = try zstd.dict.decompressUsingDDict(gpa, compressed, &ddict, original.len);
    defer gpa.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
    std.debug.print("Dictionary compress/decompress round trip OK\n", .{});
}
