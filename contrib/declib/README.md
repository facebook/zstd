# Single File Zstandard Decompression Library

The script `combine.sh` creates an _amalgamted_ source file that can be used with or without `zstd.h`. This isn't a _header-only_ file but it does offer a similar level of simplicity when integrating into a project.

Create `zstddeclib.c` from the Zstd source using:
```
cd zstd/contrib/declib
./combine.sh -r ../../lib -r ../../lib/common -r ../../lib/decompress -o zstddeclib.c zstddeclib-in.c
```
Then add the resulting file to your project (see the [example files](examples)).

`build.sh` will run the above script then compile and test the resulting library.
