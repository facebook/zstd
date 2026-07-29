//! Error handling for the zstd compression library.
//!
//! Wraps `zstd_errors.h` and provides a unified Zig `error` set that maps every
//! `ZSTD_ErrorCode` to a named error. The `check` function converts raw C
//! `size_t` return values into Zig error unions.

const std = @import("std");

/// All error codes from `ZSTD_ErrorCode` in `zstd_errors.h`.
pub const ZstdError = error{
    OutOfMemory,
    GenericError,
    PrefixUnknown,
    VersionUnsupported,
    FrameParameterUnsupported,
    FrameParameterWindowTooLarge,
    CorruptionDetected,
    ChecksumWrong,
    LiteralsHeaderWrong,
    DictionaryCorrupted,
    DictionaryWrong,
    DictionaryCreationFailed,
    ParameterUnsupported,
    ParameterCombinationUnsupported,
    ParameterOutOfBound,
    TableLogTooLarge,
    MaxSymbolValueTooLarge,
    MaxSymbolValueTooSmall,
    CannotProduceUncompressedBlock,
    StabilityConditionNotRespected,
    StageWrong,
    InitMissing,
    MemoryAllocation,
    WorkSpaceTooSmall,
    DstSizeTooSmall,
    SrcSizeWrong,
    DstBufferNull,
    NoForwardProgressDestFull,
    NoForwardProgressInputEmpty,
    FrameIndexTooLarge,
    SeekableIO,
    DstBufferWrong,
    SrcBufferWrong,
    SequenceProducerFailed,
    ExternalSequencesInvalid,
};

/// Maps a zstd `ZSTD_ErrorCode` integer to a `ZstdError` value.
pub fn errorCodeToError(code: c_int) ZstdError {
    return switch (code) {
        0 => unreachable,
        1 => error.GenericError,
        10 => error.PrefixUnknown,
        12 => error.VersionUnsupported,
        14 => error.FrameParameterUnsupported,
        16 => error.FrameParameterWindowTooLarge,
        20 => error.CorruptionDetected,
        22 => error.ChecksumWrong,
        24 => error.LiteralsHeaderWrong,
        30 => error.DictionaryCorrupted,
        32 => error.DictionaryWrong,
        34 => error.DictionaryCreationFailed,
        40 => error.ParameterUnsupported,
        41 => error.ParameterCombinationUnsupported,
        42 => error.ParameterOutOfBound,
        44 => error.TableLogTooLarge,
        46 => error.MaxSymbolValueTooLarge,
        48 => error.MaxSymbolValueTooSmall,
        49 => error.CannotProduceUncompressedBlock,
        50 => error.StabilityConditionNotRespected,
        60 => error.StageWrong,
        62 => error.InitMissing,
        64 => error.MemoryAllocation,
        66 => error.WorkSpaceTooSmall,
        70 => error.DstSizeTooSmall,
        72 => error.SrcSizeWrong,
        74 => error.DstBufferNull,
        80 => error.NoForwardProgressDestFull,
        82 => error.NoForwardProgressInputEmpty,
        100 => error.FrameIndexTooLarge,
        102 => error.SeekableIO,
        104 => error.DstBufferWrong,
        105 => error.SrcBufferWrong,
        106 => error.SequenceProducerFailed,
        107 => error.ExternalSequencesInvalid,
        else => error.GenericError,
    };
}

/// Checks a zstd `size_t` return value. If it represents an error, converts it
/// to a `ZstdError`; otherwise returns the unwrapped `usize` value.
pub fn check(result: usize) ZstdError!usize {
    if (c.ZSTD_isError(result) != 0) {
        return errorCodeToError(@intCast(c.ZSTD_getErrorCode(result)));
    }
    return result;
}

/// Checks a ZDICT `size_t` return value. If it represents an error, converts it
/// to a `ZstdError`; otherwise returns the unwrapped `usize` value.
pub fn checkDict(result: usize) ZstdError!usize {
    if (c.ZDICT_isError(result) != 0) {
        const err_name = c.ZDICT_getErrorName(result);
        _ = err_name;
        return errorCodeToError(1);
    }
    return result;
}

/// Returns the human-readable error name for a zstd result value.
pub fn errorName(result: usize) [*:0]const u8 {
    return c.ZSTD_getErrorName(result);
}

/// Returns the human-readable error string for a `ZSTD_ErrorCode`.
pub fn errorString(code: c_int) [*:0]const u8 {
    return c.ZSTD_getErrorString(code);
}

/// Returns whether a `size_t` result from zstd is an error.
pub fn isError(result: usize) bool {
    return c.ZSTD_isError(result) != 0;
}

const c = @cImport({
    @cInclude("zstd.h");
    @cInclude("zstd_errors.h");
    @cInclude("zdict.h");
});

test "ZstdError covers all error codes" {
    try std.testing.expectEqual(error.GenericError, errorCodeToError(1));
    try std.testing.expectEqual(error.PrefixUnknown, errorCodeToError(10));
    try std.testing.expectEqual(error.DstSizeTooSmall, errorCodeToError(70));
    try std.testing.expectEqual(error.GenericError, errorCodeToError(120));
}

test "check converts non-error to usize" {
    const result = check(42);
    try std.testing.expectEqual(@as(usize, 42), result catch unreachable);
}
