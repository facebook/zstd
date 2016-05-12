zstd wrapper for zlib
================================

The main objective of creating a zstd wrapper for [zlib](http://zlib.net/) is to allow a quick and smooth transition to zstd for projects already using zlib.

#### Required files

To build the zstd wrapper for zlib the following files are required:
- zlib.h
- a static or dynamic zlib library
- zlibWrapper/zstd_zlibwrapper.h
- zlibWrapper/zstd_zlibwrapper.c
- a static or dynamic zstd library

The first two files are required by all projects using zlib and they are not included with the zstd distribution.
The further files are supplied with the zstd distribution.


#### Embedding the zstd wrapper within your project

Let's assume that your project that uses zlib is compiled with:
```gcc project.o -lz```

To compile the zstd wrapper with your project you have to do the following:
- change all references with ```#include "zlib.h"``` to ```#include "zstd_zlibwrapper.h"```
- compile your project with zlib_wrapper.c and a static or dynamic zstd library

The linking should be changed to:
```gcc project.o zlib_wrapper.o -lz -lzstd```


#### Using the zstd wrapper with your project

After embedding the zstd wrapper within your project the zstd library is turned off by default.
Your project should work as before with zlib. There are two options to enable zstd compression:
- compilation with ```-DZWRAP_USE_ZSTD=1``` (or using ```#define ZWRAP_USE_ZSTD 1``` before ```#include "zstd_zlibwrapper.h"```)
- using the ```void useZSTD(int turn_on)``` function (declared in ```#include "zstd_zlibwrapper.h"```)
There is no switch for zstd decompression because zlib and zstd streams are automatically detected and decompressed using a proper library.


#### Example
We have take the file ```test\example.c``` from [the zlib library distribution](http://zlib.net/) and copied it to ```[zlibWrapper\examples\example.c](examples/example.c)```.
After compilation and execution it shows the following results: 
```
zlib version 1.2.8 = 0x1280, compile flags = 0x65
uncompress(): hello, hello!
gzread(): hello, hello!
gzgets() after gzseek:  hello!
inflate(): hello, hello!
large_inflate(): OK
after inflateSync(): hello, hello!
inflate with dictionary: hello, hello!
```
Then we have compiled the ```[example.c](examples/example.c)``` file with ```-DZWRAP_USE_ZSTD=1``` and linked with additional ```zlib_wrapper.o -lzstd```.
We have also turned of the following functions: test_gzio, test_flush, test_sync which use currently unsupported features.
After running it shows the following results:
```
zlib version 1.2.8 = 0x1280, compile flags = 0x65
uncompress(): hello, hello!
inflate(): hello, hello!
large_inflate(): OK
inflate with dictionary: hello, hello!
```
The script used for compilation can be found at [zlibWrapper\Makefile](Makefile).


#### Compatibility issues
After enabling zstd compression not all native zlib functions are supported. When calling unsupported methods they print error message and return an error value.

Supported methods:
- deflateInit
- deflate (with exception of Z_FULL_FLUSH)
- deflateSetDictionary
- deflateEnd
- deflateBound
- inflateInit
- inflate
- inflateSetDictionary
- compress
- compress2
- compressBound
- uncompress

Ignored methods (they do nothing):
- deflateParams

Unsupported methods:
- gzip file access functions
- deflateCopy
- deflateReset
- deflateTune
- deflatePending
- deflatePrime
- deflateSetHeader
- inflateGetDictionary
- inflateCopy
- inflateReset
- inflateReset2
- inflatePrime
- inflateMark
- inflateGetHeader
- inflateBackInit
- inflateBack
- inflateBackEnd
