const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const zstd_mod = b.addModule("zstd", .{
        .root_source_file = b.path("src/zstd.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = false,
    });

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

    const fmt_step = b.step("fmt", "Format source files");
    const fmt = b.addFmt(.{
        .paths = &.{ b.path("src"), b.path("build.zig") },
    });
    fmt_step.dependOn(&fmt.step);

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

    // Examples
    const examples = [_]struct { name: []const u8, file: []const u8 }{
        .{ .name = "basic", .file = "examples/basic.zig" },
        .{ .name = "streaming", .file = "examples/streaming.zig" },
        .{ .name = "decompress", .file = "examples/decompress.zig" },
        .{ .name = "compression-levels", .file = "examples/compression-levels.zig" },
        .{ .name = "dictionary", .file = "examples/dictionary.zig" },
        .{ .name = "frame-inspection", .file = "examples/frame-inspection.zig" },
        .{ .name = "advanced-parameters", .file = "examples/advanced-parameters.zig" },
        .{ .name = "custom-allocator", .file = "examples/custom-allocator.zig" },
        .{ .name = "streaming-decompress", .file = "examples/streaming-decompress.zig" },
    };

    inline for (examples) |example| {
        const run_step = b.step(
            "run-" ++ example.name,
            "Run " ++ example.name ++ " example",
        );

        const exe = b.addExecutable(.{
            .name = "example-" ++ example.name,
            .root_module = b.createModule(.{
                .root_source_file = b.path(example.file),
                .target = target,
                .optimize = optimize,
                .imports = &.{
                    .{ .name = "zstd", .module = zstd_mod },
                },
            }),
        });

        const run_exe = b.addRunArtifact(exe);
        run_step.dependOn(&run_exe.step);
        run_exe.step.dependOn(&lib.step);
    }
}
