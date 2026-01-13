const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const linkage = b.option(std.builtin.LinkMode, "linkage", "Linkage type for the library") orelse .static;
    const build_compression = b.option(bool, "ZSTD_BUILD_COMPRESSION", "BUILD COMPRESSION MODULE") orelse true;
    const build_decompression = b.option(bool, "ZSTD_BUILD_DECOMPRESSION", "BUILD DECOMPRESSION MODULE") orelse true;
    const build_dictbuilder = b.option(bool, "ZSTD_BUILD_DICTBUILDER", "BUILD DICTBUILDER MODULE") orelse true;
    const build_deprecated = b.option(bool, "ZSTD_BUILD_DEPRECATED", "BUILD DEPRECATED MODULE") orelse false;
    const build_legacy = b.option(bool, "ZSTD_BUILD_LEGACY", "BUILD LEGACY MODULE") orelse false;

    // Common flags for zstd
    const flags = &[_][]const u8{
        "-DZSTD_MULTITHREAD", // Enable multi-threading support
        "-std=c99",
    };

    const zstd_lib = b.addLibrary(.{
        .name = "zstd",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
        .linkage = linkage,
    });

    // Add include directories
    zstd_lib.root_module.addIncludePath(b.path("lib"));
    zstd_lib.root_module.addIncludePath(b.path("lib/common"));
    zstd_lib.root_module.addIncludePath(b.path("lib/compress"));
    zstd_lib.root_module.addIncludePath(b.path("lib/decompress"));
    zstd_lib.root_module.addIncludePath(b.path("lib/dictBuilder"));

    // Common sources
    zstd_lib.addCSourceFiles(.{
        .root = b.path("lib/common"),
        .files = &common_sources,
        .flags = flags,
    });

    if (build_compression) {
        zstd_lib.addCSourceFiles(.{
            .root = b.path("lib/compress"),
            .files = &compress_sources,
            .flags = flags,
        });
    }

    if (build_decompression) {
        zstd_lib.addCSourceFiles(.{
            .root = b.path("lib/decompress"),
            .files = &decompress_sources,
            .flags = flags,
        });
        if (target.result.cpu.arch == .x86_64) {
            zstd_lib.addCSourceFiles(.{
                .root = b.path("lib/decompress"),
                .files = &.{"huf_decompress_amd64.S"},
                .flags = flags,
            });
        } else {
            zstd_lib.root_module.addCMacro("ZSTD_DISABLE_ASM", "");
        }
    }

    if (build_dictbuilder) {
        zstd_lib.addCSourceFiles(.{
            .root = b.path("lib/dictBuilder"),
            .files = &dictbuilder_sources,
            .flags = flags,
        });
    }

    if (build_deprecated) {
        zstd_lib.addCSourceFiles(.{
            .root = b.path("lib/deprecated"),
            .files = &deprecated_sources,
            .flags = flags,
        });
    }

    if (build_legacy) {
        zstd_lib.root_module.addCMacro("ZSTD_LEGACY_SUPPORT", "1");
        zstd_lib.root_module.addIncludePath(b.path("lib/legacy"));
        zstd_lib.addCSourceFiles(.{
            .root = b.path("lib/legacy"),
            .files = &legacy_sources,
            .flags = flags,
        });
    }

    // Install headers
    zstd_lib.installHeader(b.path("lib/zstd.h"), "zstd.h");
    zstd_lib.installHeader(b.path("lib/zdict.h"), "zdict.h");
    zstd_lib.installHeader(b.path("lib/zstd_errors.h"), "zstd_errors.h");

    b.installArtifact(zstd_lib);

    // Create a module for other Zig projects to use
    const zstd_module = b.addModule("zstd", .{
        .root_source_file = b.path("src/zstd.zig"),
    });
    zstd_module.linkLibrary(zstd_lib);
    zstd_module.addIncludePath(b.path("lib"));
    zstd_module.addIncludePath(b.path("lib/common"));
    zstd_module.addIncludePath(b.path("lib/compress"));
    zstd_module.addIncludePath(b.path("lib/decompress"));
    zstd_module.addIncludePath(b.path("lib/dictBuilder"));
    zstd_module.addIncludePath(b.path("lib/deprecated"));
    zstd_module.addIncludePath(b.path("lib/legacy"));

    // Zig Examples
    const examples = [_]struct { name: []const u8, src: []const u8 }{
        .{ .name = "simple_compression", .src = "examples/simple_compression.zig" },
        .{ .name = "simple_decompression", .src = "examples/simple_decompression.zig" },
        .{ .name = "dictionary_compression", .src = "examples/dictionary_compression.zig" },
        .{ .name = "dictionary_decompression", .src = "examples/dictionary_decompression.zig" },
        .{ .name = "multiple_simple_compression", .src = "examples/multiple_simple_compression.zig" },
        .{ .name = "multiple_streaming_compression", .src = "examples/multiple_streaming_compression.zig" },
        .{ .name = "streaming_compression", .src = "examples/streaming_compression.zig" },
        .{ .name = "streaming_decompression", .src = "examples/streaming_decompression.zig" },
        .{ .name = "streaming_memory_usage", .src = "examples/streaming_memory_usage.zig" },
    };

    for (examples) |ex| {
        const exe = b.addExecutable(.{
            .name = ex.name,
            .root_module = b.createModule(.{
                .root_source_file = b.path(ex.src),
                .target = target,
                .optimize = optimize,
            }),
        });
        exe.root_module.addImport("zstd", zstd_module);
        exe.linkLibrary(zstd_lib);

        b.installArtifact(exe);

        const run_cmd = b.addRunArtifact(exe);
        run_cmd.step.dependOn(b.getInstallStep());
        if (b.args) |args| {
            run_cmd.addArgs(args);
        }

        const run_step = b.step(
            b.fmt("run-{s}", .{ex.name}),
            b.fmt("Run the {s} example", .{ex.name}),
        );
        run_step.dependOn(&run_cmd.step);
    }

    // Test step (Smoke test)
    const test_step = b.step("test", "Run smoke tests using examples");

    const simple_compression_exe = b.addExecutable(.{
        .name = "simple_compression",
        .root_module = b.createModule(.{
            .root_source_file = b.path("examples/simple_compression.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    simple_compression_exe.root_module.addImport("zstd", zstd_module);
    simple_compression_exe.linkLibrary(zstd_lib);

    const run_compress = b.addRunArtifact(simple_compression_exe);
    run_compress.addArgs(&.{"README.md"});

    test_step.dependOn(b.getInstallStep());

    // Docs step
    const docs_step = b.step("docs", "Emit docs");

    const docs_obj = b.addLibrary(.{
        .name = "zstd",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/zstd.zig"),
            .target = target,
            .optimize = optimize,
        }),
        .linkage = .static,
    });

    docs_obj.root_module.addIncludePath(b.path("lib"));
    docs_obj.root_module.addIncludePath(b.path("lib/common"));
    docs_obj.root_module.addIncludePath(b.path("lib/compress"));
    docs_obj.root_module.addIncludePath(b.path("lib/decompress"));
    docs_obj.root_module.addIncludePath(b.path("lib/dictBuilder"));

    docs_obj.root_module.linkLibrary(zstd_lib);

    const install_docs = b.addInstallDirectory(.{
        .source_dir = docs_obj.getEmittedDocs(),
        .install_dir = .prefix,
        .install_subdir = "docs",
    });
    docs_step.dependOn(&install_docs.step);
}

const common_sources = [_][]const u8{
    "debug.c",
    "entropy_common.c",
    "error_private.c",
    "fse_decompress.c",
    "pool.c",
    "threading.c",
    "xxhash.c",
    "zstd_common.c",
};

const compress_sources = [_][]const u8{
    "fse_compress.c",
    "hist.c",
    "huf_compress.c",
    "zstdmt_compress.c",
    "zstd_compress.c",
    "zstd_compress_literals.c",
    "zstd_compress_sequences.c",
    "zstd_compress_superblock.c",
    "zstd_double_fast.c",
    "zstd_fast.c",
    "zstd_lazy.c",
    "zstd_ldm.c",
    "zstd_opt.c",
    "zstd_preSplit.c",
};

const decompress_sources = [_][]const u8{
    "huf_decompress.c",
    "zstd_ddict.c",
    "zstd_decompress.c",
    "zstd_decompress_block.c",
};

const dictbuilder_sources = [_][]const u8{
    "cover.c",
    "divsufsort.c",
    "fastcover.c",
    "zdict.c",
};

const deprecated_sources = [_][]const u8{
    "zbuff_common.c",
    "zbuff_compress.c",
    "zbuff_decompress.c",
};

const legacy_sources = [_][]const u8{
    "zstd_v01.c",
    "zstd_v02.c",
    "zstd_v03.c",
    "zstd_v04.c",
    "zstd_v05.c",
    "zstd_v06.c",
    "zstd_v07.c",
};
