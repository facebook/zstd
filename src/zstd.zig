const builtin = @import("builtin");
const is_aarch64_macos = builtin.target.cpu.arch == .aarch64 and builtin.target.os.tag == .macos;

pub const c = if (is_aarch64_macos) @cImport({
    @cDefine("ZSTD_NO_INTRINSICS", "");
    @cDefine("ZSTD_DISABLE_ASM", "");
    @cDefine("ZSTD_STATIC_LINKING_ONLY", "");
    @cInclude("zstd.h");
    @cInclude("zstd_errors.h");
    @cInclude("zdict.h");
}) else @cImport({
    @cDefine("ZSTD_STATIC_LINKING_ONLY", "");
    @cInclude("zstd.h");
    @cInclude("zstd_errors.h");
    @cInclude("zdict.h");
});
