# zstd.zig

A Zig library for Facebook's [zstd](https://github.com/facebook/zstd) fast compression algorithm (v1.6.0). This repository provides a Zig build system (`build.zig`, `build.zig.zon`) and Zig standard bindings for the zstd C library, allowing easy integration into Zig projects.

## Documentation

You can generate the documentation locally using:

```bash
zig build docs
```

The documentation will be generated in `zig-out/docs`.

## Installation

### Zig Fetch

You can fetch the package directly using `zig fetch`:

```bash
zig fetch --save git+https://github.com/muhammad-fiaz/zstd.zig.git
```

### Build.zig.zon

Alternatively, add the dependency manually to your `build.zig.zon`:

```zig
.{
    .name = "my-project",
    .version = "0.1.0",
    .dependencies = .{
        .zstd = .{
            .url = "git+https://github.com/muhammad-fiaz/zstd.zig.git#COMMIT_HASH",
            .hash = "...", // Run `zig build` to find the correct hash
        },
    },
}
```

## Usage

### In your `build.zig`

Add `zstd` as a dependency and import the module:

```zig
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Resolve dependencies
    const zstd_dep = b.dependency("zstd", .{
        .target = target,
        .optimize = optimize,
    });
    const zstd_mod = zstd_dep.module("zstd");

    const exe = b.addExecutable(.{
        .name = "my-exe",
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Add zstd module
    exe.root_module.addImport("zstd", zstd_mod);

    // Link the library
    // Note: The module automatically links the library, but if you need strictly C access:
    // exe.linkLibrary(zstd_dep.artifact("zstd")); 

    b.installArtifact(exe);
}
```

### In your code

```zig
const std = @import("std");
const zstd = @import("zstd");

pub fn main() !void {
    const input = "Hello, Zstandard from Zig!";
    
    // Compression
    const max_dst_size = zstd.c.ZSTD_compressBound(input.len);
    const dest_buffer = try std.heap.c_allocator.alloc(u8, max_dst_size);
    defer std.heap.c_allocator.free(dest_buffer);
    
    const cSize = zstd.c.ZSTD_compress(dest_buffer.ptr, max_dst_size, input.ptr, input.len, 1);
    if (zstd.c.ZSTD_isError(cSize) != 0) {
        return error.CompressionFailed;
    }
    
    std.debug.print("Compressed size: {d}\n", .{cSize});
}
```

## features

- **Build System**: Pure Zig build system compatible with Zig 0.15+.
- **Direct Source Integration**: Uses local C sources (forked from upstream zstd) avoiding external system dependencies.
- **Configurable**: Supports standard zstd build options (Multithreading, legacy support, etc.).
- **Examples**: Includes Zig translations of standard Zstd C examples.

## Supported Platforms

- Windows / MacOS / Linux
- x86_64, aarch64, and other targets supported by Zig and Zstd.

## License

This project is double-licensed under [BSD](LICENSE) and [GPLv2](COPYING).
Original Zstd code is Copyright (c) Meta Platforms, Inc. and affiliates.
Zig bindings and build system modifications are Copyright (c) Muhammad Fiaz.
