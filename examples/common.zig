const std = @import("std");

pub fn readFile(allocator: std.mem.Allocator, filename: []const u8) ![]u8 {
    const file = try std.fs.cwd().openFile(filename, .{});
    defer file.close();

    const stat = try file.stat();
    if (stat.size > 1024 * 1024 * 1024) return error.FileTooLarge;

    return file.readToEndAlloc(allocator, @intCast(stat.size));
}

pub fn writeFile(filename: []const u8, data: []const u8) !void {
    const file = try std.fs.cwd().createFile(filename, .{});
    defer file.close();
    try file.writeAll(data);
}

pub fn createOutFilename(allocator: std.mem.Allocator, filename: []const u8) ![]u8 {
    return std.fmt.allocPrint(allocator, "{s}.zst", .{filename});
}



