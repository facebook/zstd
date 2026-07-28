const builtin = @import("builtin");

pub const c = struct {
    pub extern fn ZSTD_compress(dst: ?*anyopaque, dstCapacity: usize, src: ?*const anyopaque, srcSize: usize, compressionLevel: c_int) usize;
    pub extern fn ZSTD_decompress(dst: ?*anyopaque, dstCapacity: usize, src: ?*const anyopaque, compressedSize: usize) usize;
    pub extern fn ZSTD_getFrameContentSize(src: ?*const anyopaque, srcSize: usize) u64;
    pub extern fn ZSTD_isError(code: usize) c_uint;
    pub extern fn ZSTD_compressBound(srcSize: usize) usize;
    pub extern fn ZSTD_getErrorName(code: usize) [*:0]const u8;
};

