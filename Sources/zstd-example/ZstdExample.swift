//===----------------------------------------------------------------------===//
//
//  zstd-example
//
//  An example use of the Zstd Swift module.
//
//  Note: the package ships *two* modules built from the same C headers:
//
//    • `import libzstd` — the original raw C API (`ZSTD_createCCtx`,
//       `ZSTD_compress`, `ZSTD_CCtx_setParameter`, etc.).  Kept for source-
//       level backwards compatibility with existing Swift code.
//
//    • `import Zstd` — the modern, idiomatic API.  It cleans up the names
//       so that they are more idiomatic in Swift.  e.g. the `ZSTD_` prefix is
//       stripped and the `CCtx` / `DCtx` / `CDict` / `DDict` / `Ctx` /
//       `Dict` / `Param` / `cLevel` / `Src` abbreviations are expanded
//       into full words.
//
//       So instead of `ZSTD_createCCtx()` you write
//       `createCompressionContext()`; instead of `ZSTD_CCtx_setParameter(...)`
//       you write `setCompressionParameter(...)`; `ZSTD_compressBound(...)`
//       becomes `compressionBound(...)`; and so on.
//
//       Related functions are overloaded by Swift argument labels —
//       `compress(…)` covers the simple call,
//       `compress(_:into:capacity:from:size:)` is the sticky-parameter form,
//       `compress(_:into:capacity:from:size:dictionary:)` uses a precomputed
//       dictionary, and so on. Same for `decompress(...)`,
//       `compressStream(...)`, `dictionaryID(...)`.
//
//  Both modules resolve to the same underlying C library; you can pick
//  whichever name you prefer, or even import both.  The modern names come
//  from an `apinotes` file (`lib/Zstd.apinotes`) that the Swift importer
//  applies only to the `Zstd` module.
//
//  The renames are purely at compile-time (there are no shims or wrappers
//  involved), so the compiled code is identical.
//
//  Note: unfortunately, the opaque-context types (CCtx, DCtx, CDict, DDict,
//  threadPool, CCtx_params) cannot be imported as Swift classes — their
//  structs are forward-declared in the public header, while Swift's
//  foreign-reference machinery requires a complete definition.  They appear in
//  Swift as `OpaquePointer`, and you call free functions on them rather than
//  methods.  Use `defer { freeCompressionContext(context) }` to ensure
//  cleanup.
//
//===----------------------------------------------------------------------===//

import Foundation
import Zstd

// MARK: - Sendable pointer wrapper

// OpaquePointer isn't Sendable in Swift 6.  zstd's CDict / DDict are
// documented as safe to share across threads, so we wrap them in an
// `@unchecked Sendable` struct for capture into concurrent tasks.
struct SendableOpaquePointer: @unchecked Sendable {
    let pointer: OpaquePointer
}

// MARK: - Error helpers

enum ZstdError: Error, CustomStringConvertible {
    case operation(_ what: String, code: Int)
    case allocationFailed(_ what: String)

    var description: String {
        switch self {
        case .operation(let what, let code):
            return "\(what) failed: \(String(cString: errorName(code)))"
        case .allocationFailed(let what):
            return "\(what) returned nil — zstd allocation failed"
        }
    }
}

@discardableResult
func check(_ result: Int, _ what: String = #function) throws -> Int {
    if isError(result) != 0 {
        throw ZstdError.operation(what, code: result)
    }
    return result
}

// MARK: - Entry point

@main
struct ZstdExample {
    static func main() async throws {
        print("libzstd \(String(cString: versionString()))")
        print("compression levels: \(minimumCompressionLevel())…\(maximumCompressionLevel())  (default \(defaultCompressionLevel()))")
        print(String(repeating: "─", count: 60))

        let payload = makePayload(repetitions: 2_000)
        print("source payload: \(payload.count) bytes\n")

        try oneShotDemo(payload: payload)
        try contextDemo(payload: payload)
        try streamingDemo(payload: payload)
        try await dictionaryDemo()
    }

    static func makePayload(repetitions: Int) -> [UInt8] {
        let phrase = Array("The quick brown fox jumps over the lazy dog. ".utf8)
        var bytes: [UInt8] = []
        bytes.reserveCapacity(phrase.count * repetitions)
        for _ in 0..<repetitions { bytes.append(contentsOf: phrase) }
        return bytes
    }

    static let ratioFormatter: NumberFormatter = {
        let f = NumberFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.minimumFractionDigits = 1
        f.maximumFractionDigits = 1
        f.positiveSuffix = "x"
        return f
    }()

    static func ratio(of original: Int, to compressed: Int) -> String {
        let r = Double(original) / Double(max(compressed, 1))
        return ratioFormatter.string(from: r as NSNumber) ?? "\(r)x"
    }
}

// MARK: - Demo 1: one-shot compress / decompress

extension ZstdExample {
    /// Demonstrates the simplest possible API: compress and decompress in a single call to a top-level function.
    ///
    /// With `swift_name` annotations in the header, the C `ZSTD_compress` / `ZSTD_decompress` functions import as `compress(into:capacity:from:size:level:)` and `decompress(into:capacity:from:size:)` — call them directly, or qualify them as `Zstd.compress(…)` / `Zstd.decompress(…)` when the bare name might be ambiguous.
    static func oneShotDemo(payload: [UInt8]) throws {
        print("# one-shot (top-level functions)")

        let bound = compressionBound(payload.count)
        var compressed = [UInt8](repeating: 0, count: bound)

        let compressedSize = try check(
            payload.withUnsafeBufferPointer { source in
                compressed.withUnsafeMutableBufferPointer { destination in
                    // Note that the order of arguments is not idiomatic Swift, unfortunately.  Re-ordering arguments is not currently supported by Swift's C import machinery (it would require writing wrapper functions explicitly - this is intentionally left as an exercise for 3rd parties, since it introduces more compile-time and run-time complexity than mere symbol renames - e.g. then performance is a potential concern because the wrappers might impose some overhead if not fully inlined).
                    compress(into: destination.baseAddress!,
                             capacity: destination.count,
                             from: source.baseAddress!,
                             size: source.count,
                             level: 3)
                }
            },
            "compress"
        )
        compressed.removeLast(bound - compressedSize)

        var decompressed = [UInt8](repeating: 0, count: payload.count)
        let decompressedSize = try check(
            compressed.withUnsafeBufferPointer { source in
                decompressed.withUnsafeMutableBufferPointer { destination in
                    decompress(into: destination.baseAddress!,
                               capacity: destination.count,
                               from: source.baseAddress!,
                               size: source.count)
                }
            },
            "decompress"
        )

        precondition(decompressedSize == payload.count)
        precondition(decompressed == payload)

        print("  \(payload.count) → \(compressedSize) bytes (\(ratio(of: payload.count, to: compressedSize)))\n")
    }
}

// MARK: - Demo 2: explicit-context compress / decompress

extension ZstdExample {
    /// Demonstrates the explicit-context API.  ZSTD_CCtx and ZSTD_DCtx are opaque pointers (`OpaquePointer`); free functions operate on them.  The annotated names drop the `ZSTD_` prefix.
    static func contextDemo(payload: [UInt8]) throws {
        print("# explicit context (CCtx / DCtx)")

        guard let compressionContext = createCompressionContext() else {
            throw ZstdError.allocationFailed("createCompressionContext")
        }
        defer { _ = freeCompressionContext(compressionContext) }

        // Sticky parameters: persist for every subsequent compression on this context, so a server can configure once and reuse.
        try check(setCompressionParameter(compressionContext, .compressionLevel, to: 9))
        try check(setCompressionParameter(compressionContext, .embedChecksum,    to: true))

        let bound = compressionBound(payload.count)
        var compressed = [UInt8](repeating: 0, count: bound)

        let compressedSize = try check(
            payload.withUnsafeBufferPointer { source in
                compressed.withUnsafeMutableBufferPointer { destination in
                    compress(compressionContext,
                             into: destination.baseAddress!,
                             capacity: destination.count,
                             from: source.baseAddress!,
                             size: source.count)
                }
            }
        )
        compressed.removeLast(bound - compressedSize)

        guard let decompressionContext = createDecompressionContext() else {
            throw ZstdError.allocationFailed("createDecompressionContext")
        }
        defer { _ = freeDecompressionContext(decompressionContext) }

        var decompressed = [UInt8](repeating: 0, count: payload.count)
        let decompressedSize = try check(
            compressed.withUnsafeBufferPointer { source in
                decompressed.withUnsafeMutableBufferPointer { destination in
                    decompress(decompressionContext,
                               into: destination.baseAddress!,
                               capacity: destination.count,
                               from: source.baseAddress!,
                               size: source.count)
                }
            }
        )

        precondition(decompressedSize == payload.count)
        precondition(decompressed == payload)

        print("  level 9 + checksum: \(payload.count) → \(compressedSize) bytes (\(ratio(of: payload.count, to: compressedSize)))")
        print("  CCtx footprint: \(memoryUsage(ofCompressionContext: compressionContext)) bytes")
        print("  DCtx footprint: \(memoryUsage(ofDecompressionContext: decompressionContext)) bytes\n")
    }
}

// MARK: - Demo 3: streaming compress / decompress

extension ZstdExample {
    /// Streams the payload through the compressor in 4 kiB chunks, then streams the compressed bytes back through the decompressor.
    static func streamingDemo(payload: [UInt8]) throws {
        print("# streaming (chunked compress + decompress)")

        let chunkSize = 4_096

        guard let compressionContext = createCompressionContext() else {
            throw ZstdError.allocationFailed("createCompressionContext")
        }
        defer { _ = freeCompressionContext(compressionContext) }

        try check(setCompressionParameter(compressionContext, .compressionLevel, to: 5))

        let compressed = try streamCompress(payload, using: compressionContext, chunkSize: chunkSize)

        guard let decompressionContext = createDecompressionContext() else {
            throw ZstdError.allocationFailed("createDecompressionContext")
        }
        defer { _ = freeDecompressionContext(decompressionContext) }

        let decompressed = try streamDecompress(compressed, using: decompressionContext, chunkSize: chunkSize)

        precondition(decompressed == payload)

        let chunkCount = (payload.count + chunkSize - 1) / chunkSize
        print("  \(payload.count) → \(compressed.count) bytes streamed in \(chunkCount) chunks (\(ratio(of: payload.count, to: compressed.count)))\n")
    }

    /// Drives ZSTD_compressStream2 manually, demonstrating the `compressStream(_:output:input:endOp:)` function and the `InputBuffer` / `OutputBuffer` value types.
    /// (`compressStream` is overloaded — the 3-argument variant maps to ZSTD_compressStream and the 4-argument variant maps to ZSTD_compressStream2.)
    static func streamCompress(_ input: [UInt8],
                               using context: OpaquePointer,
                               chunkSize: Int) throws -> [UInt8] {
        let outBufferSize = compressionStreamOutputSize()
        var outBuffer = [UInt8](repeating: 0, count: outBufferSize)
        var compressed = [UInt8]()

        var sourcePosition = 0

        while true {
            let chunkEnd = min(sourcePosition + chunkSize, input.count)
            let isLast = (chunkEnd == input.count)
            let endOp: EndDirective = isLast ? .end : .continue

            let done: Bool = try input.withUnsafeBufferPointer { source in
                try outBuffer.withUnsafeMutableBufferPointer { destination -> Bool in
                    var inBuf = InputBuffer(
                        source: UnsafeRawPointer(source.baseAddress!.advanced(by: sourcePosition)),
                        size: chunkEnd - sourcePosition,
                        position: 0)

                    while true {
                        var outBuf = OutputBuffer(destination: UnsafeMutableRawPointer(destination.baseAddress!),
                                                  size: destination.count,
                                                  position: 0)

                        let remaining = try check(
                            compressStream(context,
                                           output: &outBuf,
                                           input: &inBuf,
                                           directive: endOp)
                        )

                        compressed.append(contentsOf:
                            UnsafeBufferPointer(start: destination.baseAddress, count: outBuf.position))

                        if isLast {
                            if remaining == 0 {
                                return true
                            }
                        } else {
                            if inBuf.position == inBuf.size {
                                return false
                            }
                        }
                    }
                }
            }

            if done {
                break
            }

            sourcePosition = chunkEnd
        }

        return compressed
    }

    /// Drives ZSTD_decompressStream manually.
    static func streamDecompress(_ input: [UInt8],
                                 using decompressionContext: OpaquePointer,
                                 chunkSize: Int) throws -> [UInt8] {
        let outBufferSize = decompressionStreamOutputSize()
        var outBuffer = [UInt8](repeating: 0, count: outBufferSize)
        var decompressed = [UInt8]()

        var sourcePosition = 0

        while sourcePosition < input.count {
            let chunkEnd = min(sourcePosition + chunkSize, input.count)

            try input.withUnsafeBufferPointer { source in
                try outBuffer.withUnsafeMutableBufferPointer { destination in
                    var inBuf = InputBuffer(source: UnsafeRawPointer(source.baseAddress!.advanced(by: sourcePosition)),
                                            size: chunkEnd - sourcePosition,
                                            position: 0)

                    while inBuf.position < inBuf.size {
                        var outBuf = OutputBuffer(destination: UnsafeMutableRawPointer(destination.baseAddress!),
                                                  size: destination.count,
                                                  position: 0)

                        try check(
                            decompressStream(decompressionContext, output: &outBuf, input: &inBuf)
                        )

                        decompressed.append(contentsOf: UnsafeBufferPointer(start: destination.baseAddress, count: outBuf.position))
                    }
                }
            }

            sourcePosition = chunkEnd
        }

        return decompressed
    }
}

// MARK: - Demo 4: dictionary compression and Sendable CDict

extension ZstdExample {
    /// Builds a "raw content" dictionary from a phrase, then uses it to compress and decompress short messages.  Finishes by sharing the same dictionary across four concurrent tasks — possible because the `CDict`'s underlying type is annotated `@unchecked Sendable`.
    static func dictionaryDemo() async throws {
        print("# dictionary compression with CDict / DDict")

        let dictionaryBytes = Array("The quick brown fox jumps over the lazy dog. ".utf8)

        guard let compressionDictionary = (dictionaryBytes.withUnsafeBufferPointer { bytes in
            createCompressionDictionary(from: UnsafeRawPointer(bytes.baseAddress!),
                                        size: bytes.count,
                                        level: 5)
        }) else {
            throw ZstdError.allocationFailed("createCompressionDictionary")
        }
        defer { _ = freeCompressionDictionary(compressionDictionary) }

        guard let decompressionDictionary = (dictionaryBytes.withUnsafeBufferPointer { bytes in
            createDecompressionDictionary(from: UnsafeRawPointer(bytes.baseAddress!),
                                          size: bytes.count)
        }) else {
            throw ZstdError.allocationFailed("createDecompressionDictionary")
        }
        defer { _ = freeDecompressionDictionary(decompressionDictionary) }

        print("  CDict id = \(dictionaryID(fromCompressionDictionary: compressionDictionary)), memory = \(memoryUsage(ofCompressionDictionary: compressionDictionary)) bytes")
        print("  DDict id = \(dictionaryID(fromDecompressionDictionary: decompressionDictionary)), memory = \(memoryUsage(ofDecompressionDictionary: decompressionDictionary)) bytes")

        guard let compressionContext = createCompressionContext() else {
            throw ZstdError.allocationFailed("createCompressionContext")
        }
        defer { _ = freeCompressionContext(compressionContext) }

        guard let decompressionContext = createDecompressionContext() else {
            throw ZstdError.allocationFailed("createDecompressionContext")
        }
        defer { _ = freeDecompressionContext(decompressionContext) }

        let sampleInput = Array("The quick brown fox jumps over the lazy dog.".utf8)
        let bound = compressionBound(sampleInput.count)
        var compressed = [UInt8](repeating: 0, count: bound)

        let compressedSize = try check(
            sampleInput.withUnsafeBufferPointer { source in
                compressed.withUnsafeMutableBufferPointer { destination in
                    compress(compressionContext,
                             into: destination.baseAddress!,
                             capacity: destination.count,
                             from: source.baseAddress!,
                             size: source.count,
                             dictionary: compressionDictionary)
                }
            }
        )
        compressed.removeLast(bound - compressedSize)

        var decompressed = [UInt8](repeating: 0, count: sampleInput.count)
        let decompressedSize = try check(
            compressed.withUnsafeBufferPointer { source in
                decompressed.withUnsafeMutableBufferPointer { destination in
                    decompress(decompressionContext,
                               into: destination.baseAddress!,
                               capacity: destination.count,
                               from: source.baseAddress!,
                               size: source.count,
                               dictionary: decompressionDictionary)
                }
            }
        )

        precondition(decompressedSize == sampleInput.count)
        precondition(decompressed == sampleInput)

        print("  short message: \(sampleInput.count) → \(compressedSize) bytes\n")

        // Sendable dictionary: share across concurrent tasks
        print("# concurrent CDict sharing (Sendable)")

        let sharedDictionary = SendableOpaquePointer(pointer: compressionDictionary)

        try await withThrowingTaskGroup(of: (Int, Int).self) { group in
            for taskID in 0..<4 {
                group.addTask {
                    guard let compressionContext = createCompressionContext() else {
                        throw ZstdError.allocationFailed("createCompressionContext")
                    }
                    defer { _ = freeCompressionContext(compressionContext) }

                    let phrase = "Task \(taskID): The quick brown fox jumps over the lazy dog."
                    let bytes = Array(phrase.utf8)
                    let bound = compressionBound(bytes.count)
                    var out = [UInt8](repeating: 0, count: bound)

                    let n = bytes.withUnsafeBufferPointer { source in
                        out.withUnsafeMutableBufferPointer { destination in
                            compress(compressionContext,
                                     into: destination.baseAddress!,
                                     capacity: destination.count,
                                     from: source.baseAddress!,
                                     size: source.count,
                                     dictionary: sharedDictionary.pointer)
                        }
                    }

                    return (taskID, n)
                }
            }

            for try await (taskID, size) in group {
                print("  task \(taskID) → \(size) bytes")
            }
        }
    }
}
