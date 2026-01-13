pub const c = @cImport({
    @cDefine("ZSTD_STATIC_LINKING_ONLY", "");
    @cDefine("ZSTD_LEGACY_SUPPORT", "1");
    @cInclude("zstd.h");
    @cInclude("zdict.h");
    @cInclude("zstd_errors.h");
    @cInclude("zbuff.h");
    @cInclude("zstd_legacy.h");
});
