/*
 * Umbrella header for the modern `Zstd` Swift module.
 *
 * Setting ZSTD_FOR_SWIFT_MODERN_API enables the in-header annotations
 * that benefit the modernised Swift import (e.g. enum_extensibility on
 * each ZSTD_xxx enum). The libzstd alias module uses a different umbrella
 * (libzstd_module.h) that omits this macro and thus sees raw enums.
 *
 * The macro is consumed only by zstd.h / zstd_errors.h / zdict.h, which
 * declare the relevant enums.
 */

/* Defined without a value so the Swift importer doesn't see it as a
 * top-level `let ZSTD_FOR_SWIFT_MODERN_API: Int32`.  `#if defined(...)`
 * still works the same. */
#ifndef ZSTD_FOR_SWIFT_MODERN_API
#  define ZSTD_FOR_SWIFT_MODERN_API
#endif

/* Nullability annotations and assume_nonnull are applied unconditionally
 * inside zstd.h / zdict.h (clang's pragma is file-scoped).  The legacy
 * libzstd umbrella forces ZSTD_NULLABILITY=0 to suppress them and
 * preserve its pre-annotation Swift surface. */
#include "zstd.h"
#include "zdict.h"
#include "zstd_errors.h"

/* Re-export the `ZSTD_*` / `ZDICT_*` `#define`d constants under nicer Swift
 * names.  apinotes can rename real declarations (functions, typedefs, tags)
 * but not preprocessor macros, so we wrap them as real `static const`
 * declarations here.  The `#undef`s below then hide the originals from
 * Swift's macro importer so each constant only shows up once. */
static const unsigned          versionMajor                 = ZSTD_VERSION_MAJOR;
static const unsigned          versionMinor                 = ZSTD_VERSION_MINOR;
static const unsigned          versionRelease               = ZSTD_VERSION_RELEASE;
static const unsigned          magicNumber                  = ZSTD_MAGICNUMBER;
static const unsigned          magicDictionary              = ZSTD_MAGIC_DICTIONARY;
static const unsigned          magicSkippableStart          = ZSTD_MAGIC_SKIPPABLE_START;
static const unsigned          magicSkippableMask           = ZSTD_MAGIC_SKIPPABLE_MASK;
static const int               blockSizeMax                 = ZSTD_BLOCKSIZE_MAX;
static const int               blockSizeLog2Max             = ZSTD_BLOCKSIZELOG_MAX;
static const int               defaultCompressionLevelValue = ZSTD_CLEVEL_DEFAULT;
static const unsigned long long contentSizeUnknown          = ZSTD_CONTENTSIZE_UNKNOWN;
static const unsigned long long contentSizeError            = ZSTD_CONTENTSIZE_ERROR;

#undef ZSTD_VERSION_MAJOR
#undef ZSTD_VERSION_MINOR
#undef ZSTD_VERSION_RELEASE
#undef ZSTD_MAGICNUMBER
#undef ZSTD_MAGIC_DICTIONARY
#undef ZSTD_MAGIC_SKIPPABLE_START
#undef ZSTD_MAGIC_SKIPPABLE_MASK
#undef ZSTD_BLOCKSIZE_MAX
#undef ZSTD_BLOCKSIZELOG_MAX
#undef ZSTD_CLEVEL_DEFAULT
#undef ZSTD_CONTENTSIZE_UNKNOWN
#undef ZSTD_CONTENTSIZE_ERROR

/* Bool-typed overloads of the parameter setters so call sites for the
 * flag-style parameters (.embedContentSize, .embedChecksum,
 * .embedDictionaryID, .enableLongDistanceMatching, etc.) can pass `true` /
 * `false` instead of `1` / `0`. Mapped to the same Swift name as the int-typed
 * originals via apinotes; Swift's overload resolution picks the bool variant
 * for boolean literals and the int variant for numeric literals.
 *
 * One caveat to be aware of: nothing at compile time stops one from writing
 * setCompressionParameter(cctx, .compressionLevel, to: true).  zstd would
 * receive 1 and silently set the level to 1.  The Swift type system can't tell
 * which CompressionParameter cases are boolean-valued and which take an
 * integer range, because they're all imported as cases of the same C enum.
 * The embed* naming (*Flag in the original C) is the only signal at the call
 * site. */
#include <stdbool.h>

#pragma clang assume_nonnull begin

static inline size_t Zstd_setCompressionFlag(ZSTD_CCtx* cctx,
                                             ZSTD_cParameter param,
                                             bool value) {
    return ZSTD_CCtx_setParameter(cctx, param, value ? 1 : 0);
}

static inline size_t Zstd_setDecompressionFlag(ZSTD_DCtx* dctx,
                                               ZSTD_dParameter param,
                                               bool value) {
    return ZSTD_DCtx_setParameter(dctx, param, value ? 1 : 0);
}

#pragma clang assume_nonnull end

/* Hide implementation-detail macros from Swift, which would otherwise
 * import them as top-level `let` constants (`ZSTD_NULLABILITY`, the
 * `<stdbool.h>` sentinel).  The features they control are already in
 * scope; the macro values are not part of the API.  */
#undef ZSTD_NULLABILITY
#undef __bool_true_false_are_defined
