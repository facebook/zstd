---
title: Errors
description: Error types and error handling for zstd.zig.
---

# Errors

## ZstdError

The main error set for zstd operations.

```zig
pub const ZstdError = error{
    OutOfMemory,
    Generic,
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
    WorkspaceTooSmall,
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
    BadMagic,
    MalformedFrame,
    MalformedBlock,
    MalformedCompressedBlock,
    MalformedLiteralsSection,
    MalformedLiteralsHeader,
    MalformedSequence,
    MalformedFseTable,
    MalformedFseBits,
    MalformedHuffmanTree,
    MalformedAccuracyLog,
    MissingStartBit,
    TreelessLiteralsFirst,
    RepeatModeFirst,
    InvalidBitStream,
    UnexpectedEndOfLiteralStream,
    ReadFailed,
    EndOfStream,
    OutputBufferUndersize,
    InputBufferUndersize,
    HuffmanTreeIncomplete,
    SequenceBufferUndersize,
    ReservedBitSet,
    ReservedBlock,
    WindowSizeUnknown,
    WindowOversize,
    ContentOversize,
    DictionaryIdFlagUnsupported,
};
```

## ErrorCode

Numeric error codes matching the C zstd API:

```zig
pub const ErrorCode = enum(c_int) {
    no_error = 0,
    generic = 1,
    prefix_unknown = 10,
    // ... see source for full list
};
```

## Utility Functions

| Function | Description |
|----------|-------------|
| `isError(result)` | Check if a C API result is an error |
| `getErrorCode(result)` | Extract error code from C API result |
| `errorCodeToError(code)` | Convert ErrorCode to ZstdError |
| `errorName(code)` | Get human-readable error name |

## Error Alias

```zig
pub const Error = ZstdError;
```

## Common Errors

| Error | Cause |
|-------|-------|
| `PrefixUnknown` | Data doesn't start with zstd magic number |
| `CorruptionDetected` | Data is corrupted |
| `SrcSizeWrong` | Source buffer too short |
| `DstSizeTooSmall` | Destination buffer too small |
| `OutOfMemory` | Memory allocation failed |
| `ChecksumWrong` | Frame checksum mismatch |
