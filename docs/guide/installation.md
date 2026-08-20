---
title: Installation
description: How to install and set up zstd.zig in your Zig project.
---

# Installation

## Requirements

- **Zig 0.17.0+** (dev builds) — native implementation
- No external dependencies required

::: warning
Zig 0.17.0 is currently in development. Install the latest dev build via:
```bash
scoop bucket add versions
scoop install versions/zig-dev
```
:::

## Setup

### 1. Add to build.zig.zon

Add zstd.zig as a dependency in your `build.zig.zon`:

```zig
.{
    .name = .your_project,
    .version = "0.1.0",
    .dependencies = .{
        .zstd = .{
            .url = "https://github.com/muhammad-fiaz/zstd.zig/archive/refs/heads/dev.tar.gz",
            .hash = "...",  // zig will tell you the correct hash
        },
    },
    // ...
}
```

### 2. Import in build.zig

```zig
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "my-app",
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    const zstd = b.dependency("zstd", .{});
    exe.root_module.addImport("zstd", zstd.module("zstd"));

    b.installArtifact(exe);
}
```

### 3. Use in your code

```zig
const zstd = @import("zstd");

// You're ready to go!
const compressed = try zstd.compress(allocator, data, .{});
```

## Verify Installation

Run `zig build` to fetch the dependency and compile. If it succeeds, zstd.zig is properly installed.

## Zig Version Notes

| Zig Version | Status | Notes |
|-------------|--------|-------|
| 0.17.0+ | Dev builds | Full native implementation (this library) |
| 0.16.0 | Stable | Previously available as a Zig binding to the C zstd library |

The native implementation for 0.17.0+ has no C dependency and works on all supported platforms.
