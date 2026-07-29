const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const legacy = b.option(bool, "legacy", "Enable legacy format decoding (v0.1-v0.7)") orelse false;
    const multithread = b.option(bool, "multithread", "Enable multithreaded compression (ZSTD_MULTITHREAD)") orelse true;

    const zstd_mod = b.addModule("zstd", .{
        .root_source_file = b.path("src/zstd.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    zstd_mod.addCSourceFiles(.{
        .root = b.path(""),
        .files = &.{
            "lib/common/debug.c",
            "lib/common/entropy_common.c",
            "lib/common/error_private.c",
            "lib/common/fse_decompress.c",
            "lib/common/pool.c",
            "lib/common/threading.c",
            "lib/common/xxhash.c",
            "lib/common/zstd_common.c",
        },
        .flags = &.{"-DZSTD_STATIC_LINKING_ONLY"},
    });

    zstd_mod.addCSourceFiles(.{
        .root = b.path(""),
        .files = &.{
            "lib/compress/fse_compress.c",
            "lib/compress/hist.c",
            "lib/compress/huf_compress.c",
            "lib/compress/zstd_compress.c",
            "lib/compress/zstd_compress_literals.c",
            "lib/compress/zstd_compress_sequences.c",
            "lib/compress/zstd_compress_superblock.c",
            "lib/compress/zstd_double_fast.c",
            "lib/compress/zstd_fast.c",
            "lib/compress/zstd_lazy.c",
            "lib/compress/zstd_ldm.c",
            "lib/compress/zstd_opt.c",
            "lib/compress/zstd_preSplit.c",
            "lib/compress/zstdmt_compress.c",
        },
        .flags = &.{ "-DZSTD_STATIC_LINKING_ONLY", "-DZSTD_DISABLE_DEPRECATE_WARNINGS" },
    });

    zstd_mod.addCSourceFiles(.{
        .root = b.path(""),
        .files = &.{
            "lib/decompress/huf_decompress.c",
            "lib/decompress/zstd_ddict.c",
            "lib/decompress/zstd_decompress.c",
            "lib/decompress/zstd_decompress_block.c",
        },
        .flags = &.{ "-DZSTD_STATIC_LINKING_ONLY", "-DZSTD_DISABLE_ASM" },
    });

    zstd_mod.addCSourceFiles(.{
        .root = b.path(""),
        .files = &.{
            "lib/dictBuilder/cover.c",
            "lib/dictBuilder/divsufsort.c",
            "lib/dictBuilder/fastcover.c",
            "lib/dictBuilder/zdict.c",
        },
        .flags = &.{"-DZSTD_STATIC_LINKING_ONLY"},
    });

    if (legacy) {
        zstd_mod.addCSourceFiles(.{
            .root = b.path(""),
            .files = &.{
                "lib/legacy/zstd_v01.c",
                "lib/legacy/zstd_v02.c",
                "lib/legacy/zstd_v03.c",
                "lib/legacy/zstd_v04.c",
                "lib/legacy/zstd_v05.c",
                "lib/legacy/zstd_v06.c",
                "lib/legacy/zstd_v07.c",
            },
            .flags = &.{ "-DZSTD_STATIC_LINKING_ONLY", "-DZSTD_LEGACY_SUPPORT" },
        });
    }

    zstd_mod.addIncludePath(b.path("lib"));

    if (multithread) {
        zstd_mod.addCMacro("ZSTD_MULTITHREAD", "1");
    }

    const lib = b.addLibrary(.{
        .name = "zstd",
        .root_module = zstd_mod,
    });

    b.installArtifact(lib);

    const test_step = b.step("test", "Run unit tests");
    const tests = b.addTest(.{
        .root_module = zstd_mod,
    });
    const run_tests = b.addRunArtifact(tests);
    test_step.dependOn(&run_tests.step);

    const docs_step = b.step("docs", "Generate documentation");
    const docs = b.addTest(.{
        .root_module = zstd_mod,
    });
    const install_docs = b.addInstallDirectory(.{
        .source_dir = docs.getEmittedDocs(),
        .install_dir = .prefix,
        .install_subdir = "docs",
    });
    docs_step.dependOn(&install_docs.step);

    const example_names = .{
        "simple-compress",
        "compress-bound",
        "frame-content-size",
        "cctx-parameters",
        "multithreaded-compress",
        "dctx-parameters",
        "streaming-compress-file",
        "streaming-decompress-file",
        "dictionary-compress",
        "dictionary-decompress",
        "dictionary-training",
        "error-handling",
        "benchmark-levels",
    };

    inline for (example_names) |name| {
        addExample(b, zstd_mod, name, "examples/" ++ comptime nameToFilename(name));
    }
}

fn nameToFilename(comptime name: []const u8) []const u8 {
    var buf: [64]u8 = undefined;
    var len: usize = 0;
    for (name) |c| {
        if (c == '-') {
            buf[len] = '_';
        } else {
            buf[len] = c;
        }
        len += 1;
    }
    return buf[0..len] ++ ".zig";
}

fn addExample(b: *std.Build, zstd_mod: *std.Build.Module, comptime name: []const u8, comptime src: []const u8) void {
    const exe_mod = b.createModule(.{
        .root_source_file = b.path(src),
        .target = zstd_mod.resolved_target,
        .optimize = zstd_mod.optimize,
        .imports = &.{
            .{ .name = "zstd", .module = zstd_mod },
        },
    });
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = exe_mod,
    });
    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    const step_name = b.fmt("example-{s}", .{name});
    const run_step = b.step(step_name, b.fmt("Run example: {s}", .{name}));
    run_step.dependOn(&run_cmd.step);
}
