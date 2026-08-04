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

* Zig 0.16.0 or later
* Git

### Building

```bash
zig build            # Build library
zig build test       # Run unit tests
zig build docs       # Generate documentation
```

### Running Examples

```bash
zig build example-simple-compress
zig build example-streaming-compress-file
zig build example-dictionary-training
```

## Code Style

* Follow idiomatic Zig conventions
* Use the existing code style as reference
* All public APIs must have doc comments
* Keep functions focused and small
* Use `ZstdError` for error handling

## Adding New API Wrappers

When wrapping a new zstd C API function:

1. Add the C import in the appropriate module file
2. Create a Zig wrapper function with proper error handling
3. Add doc comments explaining the function
4. Add a unit test
5. Re-export at the root level in `src/zstd.zig` if it's a convenience function
6. Update the README API reference

## Testing

All changes must pass the test suite:

```bash
zig build test
```

Tests should:
* Cover the new functionality
* Test error cases
* Use `std.testing.allocator` for memory leak detection

## Pull Request Process

1. Update documentation if needed
2. Add entries to the changelog (if applicable)
3. Ensure CI passes
4. Request review from maintainers

## Reporting Issues

* Use the GitHub issue tracker
* Include Zig version and OS
* Provide a minimal reproduction case
* Include error messages and stack traces

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (BSD + GPLv2).
