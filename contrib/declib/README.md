# Single File Zstandard Decompression Library

Create the file using the shell script:
```
cd zstd/contrib/declib
./combine.sh -r "../../lib ../../lib/common ../../lib/decompress" zstddeclib-in.c > zstddeclib.c
```
Then add the resulting file to your project (see the [test sources](tests) for examples).
