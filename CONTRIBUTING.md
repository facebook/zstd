# Contributing to zstd.zig

Thank you for your interest in contributing to zstd.zig!

## Getting Started

1. Fork the repository
2. Clone your fork
3. Create a feature branch
4. Make your changes
5. Submit a pull request

## Development Setup

### Prerequisites

* Zig 0.17.0 (development version) or later
* Git

### Installing Zig 0.17.0

Since 0.17.0 is in development, install the dev version:

```bash
# Using Scoop (Windows)
scoop bucket add versions
scoop install versions/zig-dev

# Or download from https://ziglang.org/download/
```

### Building

```bash
zig build            # Build library
zig build test       # Run all tests (35+)
zig build fmt        # Check code formatting
zig build docs       # Build documentation site
```

### Running Examples

```bash
zig build run-basic
zig build run-streaming
zig build run-decompress
zig build run-compression-levels
zig build run-dictionary
zig build run-frame-inspection
zig build run-advanced-parameters
zig build run-custom-allocator
zig build run-streaming-decompress
```

## Code Style

* Follow idiomatic Zig conventions
* Use the existing code style as reference
* All public APIs must have doc comments
* Keep functions focused and small
* Use `ZstdError` for error handling

## Architecture

This is a **pure Zig implementation** — no C dependencies or bindings.

### Source Structure

* `src/zstd.zig` — Public API facade
* `src/errors.zig` — Error codes and ZstdError set
* `src/compress.zig` — Compressor and compression options
* `src/decompress.zig` — Decompressor with safety limits
* `src/streaming.zig` — StreamCompressor/StreamDecompressor
* `src/dict.zig` — Dictionary support (CDict/DDict)
* `src/frame.zig` — Frame inspection utilities
* `src/constants.zig` — Magic numbers and sizes
* `src/version.zig` — Version information

### Key Design Principles

1. **Zero C Dependencies** — Pure Zig, builds from source
2. **Idiomatic API** — Methods on structs, not free functions
3. **Safety First** — Window limits, output bounds, reserved bit checks
4. **Backward Compatible** — Legacy functions still available

## Adding New Features

### Adding New API Functions

1. Add the function to the appropriate module file
2. Create idiomatic Zig interface with proper error handling
3. Add doc comments explaining the function
4. Add unit tests
5. Re-export at the root level in `src/zstd.zig` if it's a convenience function
6. Update the README API reference

### Example Structure

```zig
// examples/my-example.zig
const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    
    // Your example code here
}
```

## Testing

All changes must pass the test suite:

```bash
zig build test
zig build fmt
```

Tests should:
* Cover the new functionality
* Test error cases
* Use `std.testing.allocator` for memory leak detection
* Be placed in the relevant source file as `test` blocks

## Documentation

### Building Docs Locally

```bash
cd docs
npm install
npm run dev
```

### Writing Doc Pages

* Place API reference pages in `docs/api/`
* Place guide pages in `docs/guide/`
* Place example pages in `docs/examples/`
* Use Markdown with optional Vue components

## Pull Request Process

1. Update documentation if needed
2. Ensure CI passes (`zig build test`, `zig build fmt`)
3. Request review from maintainers

## Reporting Issues

* Use the GitHub issue tracker
* Include Zig version and OS
* Provide a minimal reproduction case
* Include error messages and stack traces

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (MIT).
