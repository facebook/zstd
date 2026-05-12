/*
 * Umbrella header for the legacy `libzstd` Swift module.
 *
 * Deliberately does NOT define ZSTD_FOR_SWIFT_MODERN_API so the enums in
 * zstd.h import as raw C-style values, preserving the historical Swift
 * surface that the libzstd module had before the modernisation.
 */
#include "zstd.h"
#include "zdict.h"
#include "zstd_errors.h"
