// swift-tools-version:5.5
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "zstd",
    platforms: [
        .macOS(.v10_15), .iOS(.v13), .tvOS(.v13), .watchOS(.v6)
    ],
    products: [
        // Two library products.  Each surfaces only one Swift module to its consumers:
        //
        //   • `Zstd`    — modern API (renamed functions, prefix-stripped enum cases).
        //   • `libzstd` — legacy API (raw `ZSTD_*` names) for source-level backwards
        //                 compatibility with code that predates the rename.
        //
        // A consumer that depends on the `Zstd` product can NOT `import libzstd`
        // (and vice versa), because each product is backed by a separate facade
        // target whose modulemap declares only one of the two modules.
        .library(name: "Zstd",    targets: [ "Zstd" ]),
        .library(name: "libzstd", targets: [ "libzstd" ]),
        .executable(name: "zstd-example", targets: [ "zstd-example" ]),
    ],
    targets: [
        // ── The actual C library ────────────────────────────────────────────
        // Compiles the real zstd sources once and exposes the raw headers via
        // a "private" `_ZstdCore` module that nobody is expected to import.
        // Both facade targets below depend on this one for the C symbols.
        .target(
            name: "_ZstdCore",
            path: "lib",
            sources: [ "common", "compress", "decompress", "dictBuilder" ],
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath("."),
            ]),

        // ── Modern Swift facade ─────────────────────────────────────────────
        // Owns the `Zstd_module.h` umbrella (which defines
        // `ZSTD_FOR_SWIFT_MODERN_API` before including the real headers) and
        // its accompanying `Zstd.apinotes`.  The umbrella + apinotes pair is the
        // only thing that turns "raw zstd headers" into the modern Swift API.
        .target(
            name: "Zstd",
            dependencies: [ "_ZstdCore" ],
            path: "Sources/Zstd",
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("../../lib"),  // so the umbrella can find zstd.h
            ]),

        // ── Legacy Swift facade ─────────────────────────────────────────────
        // Same headers as `Zstd`, but with an umbrella that doesn't define the
        // modern-API macro and no apinotes file, so the raw C names come through.
        .target(
            name: "libzstd",
            dependencies: [ "_ZstdCore" ],
            path: "Sources/libzstd",
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("../../lib"),
            ]),

        .executableTarget(
            name: "zstd-example",
            dependencies: [ "Zstd" ],
            path: "Sources/zstd-example"),
    ],
    swiftLanguageVersions: [.v5],
    cLanguageStandard: .gnu11,
    cxxLanguageStandard: .gnucxx14
)
