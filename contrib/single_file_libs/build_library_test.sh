#!/bin/sh

# Where to find the sources (only used to copy zstd.h)
ZSTD_SRC_ROOT="../../lib"

# Temporary compiled binary
OUT_FILE="tempbin"

# Optional temporary compiled WebAssembly
OUT_WASM="temp.wasm"

# Amalgamate the sources
./create_single_file_library.sh
# Did combining work?
if [ $? -ne 0 ]; then
  echo "Single file library creation script: FAILED"
  exit 1
fi
echo "Single file library creation script: PASSED"

# Copy the header to here (for the tests)
cp "$ZSTD_SRC_ROOT/zstd.h" zstd.h

# Compile the generated output
cc -Wall -Wextra -Werror -pthread -I. -Os -g0 -o $OUT_FILE zstd.c examples/roundtrip.c
# Did compilation work?
if [ $? -ne 0 ]; then
  echo "Compiling roundtrip.c: FAILED"
  exit 1
fi
echo "Compiling roundtrip.c: PASSED"

# Run then delete the compiled output
./$OUT_FILE
retVal=$?
rm -f $OUT_FILE
# Did the test work?
if [ $retVal -ne 0 ]; then
  echo "Running roundtrip.c: FAILED"
  exit 1
fi
echo "Running roundtrip.c: PASSED"

# Is Emscripten available?
which emcc > /dev/null
if [ $? -ne 0 ]; then
  echo "(Skipping Emscripten test)"
else
  # Compile the the same example as above
  CC_FLAGS="-Wall -Wextra -Werror -Os -g0 -flto"
  emcc $CC_FLAGS -s WASM=1 -I. -o $OUT_WASM zstd.c examples/roundtrip.c
  # Did compilation work?
  if [ $? -ne 0 ]; then
    echo "Compiling emscripten.c: FAILED"
    exit 1
  fi
  echo "Compiling emscripten.c: PASSED"
  rm -f $OUT_WASM
fi

exit 0
