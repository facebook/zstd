#!/bin/sh
set -e

echo Detected build type: make

# Create required directories
mkdir -p bin/dll bin/static bin/example bin/include

copy_file() {
    cp $1 "$2" || { echo "Failed to copy $1"; exit 1; }
}

# Copy common files using a function. Exits immediately on failure.
copy_file "tests/fullbench.c"              "bin/example/"
copy_file "programs/datagen.c"             "bin/example/"
copy_file "programs/datagen.h"             "bin/example/"
copy_file "programs/util.h"                "bin/example/"
copy_file "programs/platform.h"            "bin/example/"
copy_file "lib/common/mem.h"               "bin/example/"
copy_file "lib/common/zstd_internal.h"     "bin/example/"
copy_file "lib/common/error_private.h"     "bin/example/"
copy_file "lib/common/xxhash.h"            "bin/example/"
copy_file "lib/dll/example/Makefile"        "bin/example/"
copy_file "lib/dll/example/fullbench-dll.sln"    "bin/example/"
copy_file "lib/dll/example/fullbench-dll.vcxproj" "bin/example/"
copy_file "lib/zstd.h"        "bin/include/"
copy_file "lib/zstd_errors.h" "bin/include/"
copy_file "lib/zdict.h"       "bin/include/"

# Copy build-specific files
echo Copying Make build artifacts...
copy_file "lib/libzstd.a" "bin/static/libzstd.a"

DYLIB_FULL=$(ls lib/libzstd.*.*.*.dylib 2>/dev/null | head -1)
if [ -z "$DYLIB_FULL" ]; then
    echo "Error: could not find libzstd.X.Y.Z.dylib in lib/"
    exit 1
fi
DYLIB_NAME=$(basename "$DYLIB_FULL")
copy_file "$DYLIB_FULL" "bin/dll/"

MAJOR=$(echo "$DYLIB_NAME" | sed 's/libzstd\.\([0-9]*\)\..*/\1/')
ln -sf "$DYLIB_NAME" "bin/dll/libzstd.${MAJOR}.dylib"
ln -sf "$DYLIB_NAME" "bin/dll/libzstd.dylib"

copy_file "programs/zstd" "bin/zstd"
copy_file "lib/dll/example/README.md" "bin/"

echo Build package created successfully for make build!
