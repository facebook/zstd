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
    var flags_list = std.ArrayList([]const u8).initCapacity(b.allocator, 8) catch @panic("OOM");
    defer flags_list.deinit(b.allocator);

    flags_list.append(b.allocator, "-std=c99") catch @panic("OOM");

    // Only enable multithread if NOT freestanding
    if (target.result.os.tag != .freestanding) {
        flags_list.append(b.allocator, "-DZSTD_MULTITHREAD") catch @panic("OOM");
    } else {
        // Freestanding specific flags
        flags_list.append(b.allocator, "-fno-sanitize=all") catch @panic("OOM");
        flags_list.append(b.allocator, "-DXXH_NAMESPACE=ZSTD_") catch @panic("OOM");
    }

    const flags = flags_list.toOwnedSlice(b.allocator) catch @panic("OOM");

    const zstd_lib = b.addLibrary(.{
        .name = "zstd",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = (target.result.os.tag != .freestanding),
        }),
        .linkage = linkage,
    });

    // Handle Freestanding Targets (Mock Headers)
    if (target.result.os.tag == .freestanding) {
        const write_headers = b.addWriteFiles();

        // string.h
        _ = write_headers.add("string.h",
            \\#ifndef MOCK_STRING_H
            \\#define MOCK_STRING_H
            \\#include <stddef.h>
            \\#define memcpy(d,s,n) __builtin_memcpy(d,s,n)
            \\#define memset(d,v,n) __builtin_memset(d,v,n)
            \\#define memmove(d,s,n) __builtin_memmove(d,s,n)
            \\#define memcmp(a,b,n) __builtin_memcmp(a,b,n)
            \\#define strlen(s) __builtin_strlen(s)
            \\#endif
        );

        // stdlib.h (Declarations only, implementation must be provided by user/kernel if used)
        _ = write_headers.add("stdlib.h",
            \\#ifndef MOCK_STDLIB_H
            \\#define MOCK_STDLIB_H
            \\#include <stddef.h>
            \\extern void* malloc(size_t size);
            \\extern void free(void* ptr);
            \\extern void* calloc(size_t nmemb, size_t size);
            \\extern void* realloc(void* ptr, size_t size);
            \\#endif
        );

        // stdio.h
        _ = write_headers.add("stdio.h",
            \\#ifndef MOCK_STDIO_H
            \\#define MOCK_STDIO_H
            \\#include <stddef.h>
            \\typedef struct FILE FILE;
            \\#define stderr ((FILE*)0)
            \\#define stdout ((FILE*)1)
            \\extern int fprintf(FILE* stream, const char* format, ...);
            \\extern int printf(const char* format, ...);
            \\#endif
        );

        // assert.h
        _ = write_headers.add("assert.h", "#define assert(x) ((void)0)");

        // limits.h
        _ = write_headers.add("limits.h",
            \\#ifndef MOCK_LIMITS_H
            \\#define MOCK_LIMITS_H
            \\#define INT_MAX   2147483647
            \\#define INT_MIN   (-INT_MAX - 1)
            \\#define UINT_MAX  4294967295U
            \\#endif
        );

        // time.h
        _ = write_headers.add("time.h",
            \\#ifndef MOCK_TIME_H
            \\#define MOCK_TIME_H
            \\typedef long time_t;
            \\typedef long clock_t;
            \\#define CLOCKS_PER_SEC 1000
            \\static inline clock_t clock(void) { return 0; }
            \\static inline time_t time(time_t* timer) { if(timer) *timer=0; return 0; }
            \\#endif
        );

        // math.h (Minimal stub, user should link libm if needed, but this satisfies includes)
        _ = write_headers.add("math.h", "");

        // pthread.h (Stub for threading)
        _ = write_headers.add("pthread.h", "");

        // Add mock headers to include path
        zstd_lib.root_module.addIncludePath(write_headers.getDirectory());

        // Disable Assembly for safety in freestanding defaults
        zstd_lib.root_module.addCMacro("ZSTD_DISABLE_ASM", "");
    }

    // Add include directories
    zstd_lib.root_module.addIncludePath(b.path("lib"));
    zstd_lib.root_module.addIncludePath(b.path("lib/common"));
    zstd_lib.root_module.addIncludePath(b.path("lib/compress"));
    zstd_lib.root_module.addIncludePath(b.path("lib/decompress"));
    zstd_lib.root_module.addIncludePath(b.path("lib/dictBuilder"));

    // On Apple ARM (macOS aarch64) disable intrinsics/ASM to avoid
    // NEON/assembly builtin issues when compiling vendored C sources with Zig.
    if (target.result.os.tag == .macos and target.result.cpu.arch == .aarch64) {
        zstd_lib.root_module.addCMacro("ZSTD_NO_INTRINSICS", "");
        zstd_lib.root_module.addCMacro("ZSTD_DISABLE_ASM", "");
    }

    // Common sources
    zstd_lib.root_module.addCSourceFiles(.{
        .root = b.path("lib/common"),
        .files = &common_sources,
        .flags = flags,
    });

    if (build_compression) {
        zstd_lib.root_module.addCSourceFiles(.{
            .root = b.path("lib/compress"),
            .files = &compress_sources,
            .flags = flags,
        });
    }

    if (build_decompression) {
        zstd_lib.root_module.addCSourceFiles(.{
            .root = b.path("lib/decompress"),
            .files = &decompress_sources,
            .flags = flags,
        });

        // ASM support only if NOT freestanding (or carefully checked)
        if (target.result.os.tag != .freestanding and target.result.cpu.arch == .x86_64) {
            zstd_lib.root_module.addCSourceFiles(.{
                .root = b.path("lib/decompress"),
                .files = &.{"huf_decompress_amd64.S"},
                .flags = flags,
            });
        } else {
            zstd_lib.root_module.addCMacro("ZSTD_DISABLE_ASM", "");
        }
    }

    if (build_dictbuilder and target.result.os.tag != .freestanding) {
        // DictBuilder often needs math/stdlib, might fail in pure freestanding without hard-float/libm
        zstd_lib.root_module.addCSourceFiles(.{
            .root = b.path("lib/dictBuilder"),
            .files = &dictbuilder_sources,
            .flags = flags,
        });
    }

    if (build_deprecated) {
        zstd_lib.root_module.addCSourceFiles(.{
            .root = b.path("lib/deprecated"),
            .files = &deprecated_sources,
            .flags = flags,
        });
    }

    if (build_legacy) {
        zstd_lib.root_module.addCMacro("ZSTD_LEGACY_SUPPORT", "1");
        zstd_lib.root_module.addIncludePath(b.path("lib/legacy"));
        zstd_lib.root_module.addCSourceFiles(.{
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

    // Ensure module also sees the mock headers for translate-c
    if (target.result.os.tag == .freestanding) {
        const write_headers_mod = b.addWriteFiles();
        _ = write_headers_mod.add("string.h", "#include <stddef.h>\n#define memcpy(d,s,n) __builtin_memcpy(d,s,n)\n#define memset(d,v,n) __builtin_memset(d,v,n)\n#define memmove(d,s,n) __builtin_memmove(d,s,n)\n#define memcmp(a,b,n) __builtin_memcmp(a,b,n)\n#define strlen(s) __builtin_strlen(s)");
        _ = write_headers_mod.add("stdlib.h", "#include <stddef.h>\nextern void* malloc(size_t s);\nextern void free(void* p);\nextern void* calloc(size_t n, size_t s);\nextern void* realloc(void* p, size_t s);");
        _ = write_headers_mod.add("stdio.h", "typedef struct FILE FILE;");
        _ = write_headers_mod.add("assert.h", "#define assert(x) ((void)0)");
        _ = write_headers_mod.add("limits.h", "#define INT_MAX 2147483647");
        _ = write_headers_mod.add("time.h", "typedef long time_t; typedef long clock_t; #define CLOCKS_PER_SEC 1000"); // Minimal inline stub
        _ = write_headers_mod.add("math.h", "");
        _ = write_headers_mod.add("pthread.h", "");
        zstd_module.addIncludePath(write_headers_mod.getDirectory());
    }

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

    if (target.result.os.tag != .freestanding) {
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
            exe.root_module.linkLibrary(zstd_lib);

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
    }

    // Test step
    const test_step = b.step("test", "Run smoke tests using examples");

    // Skip tests for freestanding
    if (target.result.os.tag != .freestanding) {
        const simple_compression_exe = b.addExecutable(.{
            .name = "simple_compression",
            .root_module = b.createModule(.{
                .root_source_file = b.path("examples/simple_compression.zig"),
                .target = target,
                .optimize = optimize,
            }),
        });
        simple_compression_exe.root_module.addImport("zstd", zstd_module);
        simple_compression_exe.root_module.linkLibrary(zstd_lib);

        const run_compress = b.addRunArtifact(simple_compression_exe);
        run_compress.addArgs(&.{"README.md"});

        test_step.dependOn(b.getInstallStep());
    }

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

    // Add same headers to docs object
    if (target.result.os.tag == .freestanding) {
        const write_headers_docs = b.addWriteFiles();
        _ = write_headers_docs.add("string.h", "");
        _ = write_headers_docs.add("stdlib.h", "");
        _ = write_headers_docs.add("stdio.h", "");
        _ = write_headers_docs.add("assert.h", "");
        _ = write_headers_docs.add("limits.h", "");
        _ = write_headers_docs.add("time.h", "");
        _ = write_headers_docs.add("math.h", "");
        _ = write_headers_docs.add("pthread.h", "");
        docs_obj.root_module.addIncludePath(write_headers_docs.getDirectory());
    }

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
