//! Version and capability information for the zstd library.
//!
//! Wraps `ZSTD_versionNumber`, `ZSTD_versionString`, `ZSTD_minCLevel`,
//! `ZSTD_maxCLevel`, and `ZSTD_defaultCLevel` from `zstd.h`.

const c = @cImport({
    @cInclude("zstd.h");
});

/// Returns the runtime library version as `(MAJOR*100*100 + MINOR*100 + RELEASE)`.
///
/// For example, version 1.6.0 returns `10600`.
pub fn versionNumber() u32 {
    return @intCast(c.ZSTD_versionNumber());
}

/// Returns the runtime library version as a human-readable string, e.g. `"1.6.0"`.
pub fn versionString() [*:0]const u8 {
    return c.ZSTD_versionString();
}

/// Returns the minimum negative compression level allowed.
///
/// Requires v1.4.0+.
pub fn minCLevel() i32 {
    return @intCast(c.ZSTD_minCLevel());
}

/// Returns the maximum compression level available.
pub fn maxCLevel() i32 {
    return @intCast(c.ZSTD_maxCLevel());
}

/// Returns the default compression level (`ZSTD_CLEVEL_DEFAULT`, which is 3).
///
/// Requires v1.5.0+.
pub fn defaultCLevel() i32 {
    return @intCast(c.ZSTD_defaultCLevel());
}

test "version info is sane" {
    const num = versionNumber();
    try std.testing.expect(num >= 10600);
    const str = versionString();
    try std.testing.expect(str[0] != 0);
}

test "compression level bounds" {
    const lo = minCLevel();
    const hi = maxCLevel();
    const def = defaultCLevel();
    try std.testing.expect(lo <= 0);
    try std.testing.expect(hi >= 3);
    try std.testing.expect(def >= lo and def <= hi);
}

const std = @import("std");
