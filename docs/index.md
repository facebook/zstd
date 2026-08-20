---
layout: home
title: zstd.zig
titleTemplate: Native Zig Compression Library

hero:
  name: zstd.zig
  text: Native Zig Compression Library
  tagline: "A complete native Zig implementation of Zstandard compression. No C bindings, no dependencies. Supports compression, decompression, streaming, and dictionary-based operations for Zig 0.17.0+ (dev builds)."
  actions:
    - theme: brand
      text: Get Started
      link: /guide/getting-started
    - theme: alt
      text: API Reference
      link: /api/
    - theme: alt
      text: GitHub
      link: https://github.com/muhammad-fiaz/zstd.zig

features:
  - title: Pure Zig Implementation
    details: "Complete native Zig reimplementation of Zstandard. No C bindings, no external dependencies. Every byte is Zig."
  - title: One-Shot Compression
    details: "Simple compress and decompress functions with options structs. Pass .{ .level = .fastest } or .{ .level = .best } for quick control."
  - title: Reusable Contexts
    details: "Compressor and Decompressor types that can be initialized once and reused across multiple operations. Set parameters, reset, and compress again."
  - title: Streaming Support
    details: "StreamCompressor and StreamDecompressor for chunk-based processing. Handle data that doesn't fit in memory with incremental compression."
  - title: Dictionary Compression
    details: "CDict and DDict types for dictionary-based compression. Train dictionaries from samples, achieve better ratios on similar data."
  - title: Frame Inspection
    details: "Frame namespace for inspecting zstd frames: check magic number, content size, compressed size, and dictionary ID without decompressing."
  - title: Cross-Platform
    details: "Works on Linux, Windows, macOS, and FreeBSD. Supports both 32-bit and 64-bit architectures including aarch64."
  - title: Idiomatic Zig API
    details: "Options structs, enums, and comptime features. The API feels natural in Zig with named parameters and clean error handling."
---
