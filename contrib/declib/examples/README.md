# Single File ZStandard Examples

The examples `#include` the generated `zstddeclib.c` directly but work equally as well when including `zstd.h` and compiling the amalgamated source separately.

`simple.c` is the most basic example of decompressing content and verifying the result.

`emscripten.c` is a bare-bones [Emscripten](https://github.com/emscripten-core/emscripten) compiled WebGL demo using Zstd to further compress a DXT1 texture. The 256x256 texture would normally be 32kB, but even when bundled with the Zstd decompressor the resulting WebAssembly weighs in at 45kB (`shell.html` is a support file to run the Wasm).
