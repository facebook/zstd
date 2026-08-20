---
title: Streaming
description: Process large data with chunk-based streaming compression and decompression.
---

# Streaming

For data that doesn't fit in memory, use `StreamCompressor` and `StreamDecompressor`.

## Streaming Compression

```zig
const zstd = @import("zstd");

var comp = zstd.StreamCompressor.init(allocator, .{
    .level = .default,
});
defer comp.deinit();

var output: [zstd.recommendedCOutSize()]u8 = undefined;

// Feed data in chunks
const r1 = try comp.compressChunk(chunk1, &output, .@"continue");
if (r1.bytes_written > 0) {
    try doSomething(output[0..r1.bytes_written]);
}

const r2 = try comp.compressChunk(chunk2, &output, .@"continue");

// Finalize the stream
const final = try comp.endStream(&output);
if (final.bytes_written > 0) {
    try doSomething(output[0..final.bytes_written]);
}
```

## Streaming Decompression

```zig
var decomp = zstd.StreamDecompressor.init(allocator, .{});
defer decomp.deinit();

var output: [zstd.recommendedDOutSize()]u8 = undefined;

const r1 = try decomp.decompressChunk(compressed_chunk1, &output);
try processOutput(output[0..r1.bytes_written]);

const r2 = try decomp.decompressChunk(compressed_chunk2, &output);
try processOutput(output[0..r2.bytes_written]);
```

## Recommended Buffer Sizes

```zig
const in_size = zstd.recommendedCInSize();   // 128 KB
const out_size = zstd.recommendedCOutSize();  // 128 KB + 6
const d_in = zstd.recommendedDInSize();       // 128 KB
const d_out = zstd.recommendedDOutSize();     // 128 KB
```

## EndDirective

Control when the stream finishes:

| Directive | Description |
|-----------|-------------|
| `.@"continue"` | Keep the stream open |
| `.flush` | Flush buffered data |
| `.end` | Finalize and close the stream |

## Resetting

Reset a stream compressor to reuse it:

```zig
comp.reset();
// Ready for a new compression job
```

## Parameters

```zig
comp.setParameter(.compression_level, 5);
comp.setParameter(.checksum_flag, 1);
```
