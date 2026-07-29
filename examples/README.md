# zstd.zig Examples

Runnable examples demonstrating every part of the zstd.zig API.

## Running Examples

```bash
zig build example-<name>
```

For example:

```bash
zig build example-simple-compress
```

## Available Examples

### simple_compress

Basic one-shot compression and decompression round trip.

```bash
zig build example-simple-compress
```

### compress_bound

Query the worst-case compressed size before allocating.

```bash
zig build example-compress-bound
```

### frame_content_size

Inspect frame headers: content size, compressed size, and frame detection.

```bash
zig build example-frame-content-size
```

### error_handling

Handle zstd errors gracefully with the unified Zig error set.

```bash
zig build example-error-handling
```

### cctx_parameters

Set and query compression parameters on an explicit Compressor context.

```bash
zig build example-cctx-parameters
```

### dctx_parameters

Set and query decompression parameters on an explicit Decompressor context.

```bash
zig build example-dctx-parameters
```

### multithreaded_compress

Enable multithreaded compression with nb_workers.

```bash
zig build example-multithreaded-compress
```

### streaming_compress_file

Chunk by chunk streaming compression with StreamingCompressor.

```bash
zig build example-streaming-compress-file
```

### streaming_decompress_file

Chunk by chunk streaming decompression with StreamingDecompressor.

```bash
zig build example-streaming-decompress-file
```

### dictionary_compress

Compress and decompress using a pre-loaded dictionary (CDict/DDict).

```bash
zig build example-dictionary-compress
```

### dictionary_decompress

Decompress data that was compressed with a dictionary.

```bash
zig build example-dictionary-decompress
```

### dictionary_training

Train a compression dictionary from sample data using ZDICT.

```bash
zig build example-dictionary-training
```

### benchmark_levels

Compare compression ratios across different compression levels.

```bash
zig build example-benchmark-levels
```
