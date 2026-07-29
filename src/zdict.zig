//! Dictionary builder API (ZDICT).
//!
//! Wraps `ZDICT_trainFromBuffer`, `ZDICT_finalizeDictionary`,
//! `ZDICT_getDictID`, `ZDICT_getDictHeaderSize`, `ZDICT_isError`,
//! `ZDICT_getErrorName`, and the `ZDICT_params_t`, `ZDICT_cover_params_t`,
//! and `ZDICT_fastCover_params_t` types from `zdict.h`.

const std = @import("std");
const errors = @import("errors.zig");
const ZstdError = errors.ZstdError;

const c = @cImport({
    @cInclude("zdict.h");
    @cInclude("zstd.h");
});

/// Parameters for `finalizeDictionary`.
pub const DictParams = struct {
    compression_level: i32 = 0,
    notification_level: u32 = 0,
    dict_id: u32 = 0,
};

/// Parameters for the COVER dictionary training algorithm.
pub const CoverParams = struct {
    k: u32 = 0,
    d: u32 = 0,
    steps: u32 = 0,
    nb_threads: u32 = 0,
    split_point: f64 = 0,
    shrink_dict: u32 = 0,
    shrink_dict_max_regression: u32 = 0,
    z_params: DictParams = .{},
};

/// Parameters for the fastCover dictionary training algorithm.
pub const FastCoverParams = struct {
    k: u32 = 0,
    d: u32 = 0,
    f: u32 = 0,
    steps: u32 = 0,
    nb_threads: u32 = 0,
    split_point: f64 = 0,
    accel: u32 = 0,
    shrink_dict: u32 = 0,
    shrink_dict_max_regression: u32 = 0,
    z_params: DictParams = .{},
};

/// Trains a dictionary from an array of samples.
///
/// This wraps `ZDICT_trainFromBuffer`. Redirects towards
/// `ZDICT_optimizeTrainFromBuffer_fastCover` single-threaded with d=8, steps=4,
/// f=20, and accel=1.
///
/// Samples must be stored concatenated in a single flat buffer `samples_buffer`,
/// with `sample_sizes` providing the size of each sample in order.
///
/// The resulting dictionary will be allocated with `allocator` and returned.
/// Returns an error if there are not enough samples or if the data is
/// uncompressible.
///
/// Dictionary training uses about 6 MB of memory internally.
pub fn trainFromSamples(
    allocator: std.mem.Allocator,
    samples_buffer: []const u8,
    sample_sizes: []const usize,
    dict_capacity: usize,
) ZstdError![]u8 {
    const dict_buf = try allocator.alloc(u8, dict_capacity);
    errdefer allocator.free(dict_buf);

    const written = errors.checkDict(c.ZDICT_trainFromBuffer(
        dict_buf.ptr,
        dict_buf.len,
        samples_buffer.ptr,
        sample_sizes.ptr,
        @intCast(sample_sizes.len),
    )) catch |err| {
        allocator.free(dict_buf);
        return err;
    };
    return allocator.realloc(dict_buf, written) catch dict_buf;
}

/// Finalizes a dictionary by adding headers and statistics to raw content.
///
/// This wraps `ZDICT_finalizeDictionary`. Given custom content as a basis
/// for a dictionary and a set of samples, it adds the zstd dictionary header
/// (magic number, dictionary ID, and entropy tables).
///
/// `dict_content` is the raw dictionary content. `samples_buffer` and
/// `sample_sizes` describe the training samples. `max_dict_size` is the
/// maximum size of the resulting dictionary.
pub fn finalizeDictionary(
    allocator: std.mem.Allocator,
    dict_content: []const u8,
    samples_buffer: []const u8,
    sample_sizes: []const usize,
    max_dict_size: usize,
    params: DictParams,
) ZstdError![]u8 {
    const dst = try allocator.alloc(u8, max_dict_size);
    errdefer allocator.free(dst);

    const c_params = c.ZDICT_params_t{
        .compressionLevel = @intCast(params.compression_level),
        .notificationLevel = params.notification_level,
        .dictID = params.dict_id,
    };

    const written = errors.checkDict(c.ZDICT_finalizeDictionary(
        dst.ptr,
        dst.len,
        dict_content.ptr,
        dict_content.len,
        samples_buffer.ptr,
        sample_sizes.ptr,
        @intCast(sample_sizes.len),
        c_params,
    )) catch |err| {
        allocator.free(dst);
        return err;
    };
    return allocator.realloc(dst, written) catch dst;
}

/// Returns the dictionary ID stored within a dictionary buffer.
///
/// This wraps `ZDICT_getDictID`. Returns 0 if the buffer is not a valid
/// dictionary.
pub fn getDictID(dict_buffer: []const u8) u32 {
    return @intCast(c.ZDICT_getDictID(dict_buffer.ptr, dict_buffer.len));
}

/// Returns the size of the dictionary header.
///
/// This wraps `ZDICT_getDictHeaderSize`. Returns a zstd error code on failure.
pub fn getDictHeaderSize(dict_buffer: []const u8) ZstdError!usize {
    return errors.check(c.ZDICT_getDictHeaderSize(
        dict_buffer.ptr,
        dict_buffer.len,
    ));
}

/// Returns whether a `size_t` result from ZDICT is an error.
pub fn isDictError(result: usize) bool {
    return c.ZDICT_isError(result) != 0;
}

/// Returns the human-readable error name for a ZDICT result value.
pub fn dictErrorName(result: usize) [*:0]const u8 {
    return c.ZDICT_getErrorName(result);
}

test "trainFromSamples with synthetic data" {
    var prng = std.Random.DefaultPrng.init(42);
    const random = prng.random();

    const sample_count = 100;
    const sample_len = 64;
    const total_len = sample_count * sample_len;

    var samples = try std.testing.allocator.alloc(u8, total_len);
    defer std.testing.allocator.free(samples);

    var sizes = try std.testing.allocator.alloc(usize, sample_count);
    defer std.testing.allocator.free(sizes);

    for (0..sample_count) |i| {
        const start = i * sample_len;
        for (0..sample_len) |j| {
            samples[start + j] = random.int(u8);
        }
        sizes[i] = sample_len;
    }

    const dict = trainFromSamples(std.testing.allocator, samples, sizes, 1024) catch return;
    defer std.testing.allocator.free(dict);

    try std.testing.expect(dict.len > 0);
}
