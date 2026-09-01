/*
 * Umbrella header for the legacy `libzstd` Swift module.
 *
 * Deliberately does NOT define ZSTD_FOR_SWIFT_MODERN_API so the enums in
 * zstd.h import as raw C-style values, preserving the historical Swift
 * surface that the libzstd module had before the modernisation.
 *
 * Also forces ZSTD_NULLABILITY to 0 so the `_Nullable` / `_Nonnull`
 * annotations and the `assume_nonnull` pragma added later are not
 * applied — pointers continue to import as implicitly-unwrapped
 * optionals, matching the original libzstd Swift surface exactly.
 */
#define ZSTD_NULLABILITY 0
#include "zstd.h"
#include "zdict.h"
#include "zstd_errors.h"
