Command Line Interface for Zstandard library
============================================

Command Line Interface (CLI) can be created using the `make` command without any additional parameters.
There are however other Makefile targets that create different variations of CLI:
- `zstd` : default CLI supporting gzip-like arguments; includes dictionary builder, benchmark, and support for decompression of legacy zstd versions
- `zstd32` : Same as `zstd`, but forced to compile in 32-bits mode
- `zstd_nolegacy` : Same as `zstd` except of support for decompression of legacy zstd versions
- `zstd-small` : CLI optimized for minimal size; without dictionary builder, benchmark, and support for decompression of legacy zstd versions
- `zstd-compress` : compressor-only version of CLI; without dictionary builder, benchmark, and support for decompression of legacy zstd versions
- `zstd-decompress` : decompressor-only version of CLI; without dictionary builder, benchmark, and support for decompression of legacy zstd versions


#### Compilation variables
`zstd` tries to detect and use the following features automatically :

- __HAVE_THREAD__ : multithreading is automatically enabled when `pthread` is detected.
  It's possible to disable multithread support, by either compiling `zstd-nomt` target or using HAVE_THREAD=0 variable.
  Example : make zstd HAVE_THREAD=0
  It's also possible to force compilation with multithread support, using HAVE_THREAD=1.
  In which case, linking stage will fail if `pthread` library cannot be found.
  This might be useful to prevent silent feature disabling.

- __HAVE_ZLIB__ : `zstd` can compress and decompress files in `.gz` format.
  This is done through command `--format=gzip`.
  Alternatively, symlinks named `gzip` or `gunzip` will mimic intended behavior.
  .gz support is automatically enabled when `zlib` library is detected at build time.
  It's possible to disable .gz support, by either compiling `zstd-nogz` target or using HAVE_ZLIB=0 variable.
  Example : make zstd HAVE_ZLIB=0
  It's also possible to force compilation with zlib support, using HAVE_ZLIB=1.
  In which case, linking stage will fail if `zlib` library cannot be found.
  This might be useful to prevent silent feature disabling.


#### Aggregation of parameters
CLI supports aggregation of parameters i.e. `-b1`, `-e18`, and `-i1` can be joined into `-b1e18i1`.


#### Dictionary builder in Command Line Interface
Zstd offers a training mode, which can be used to tune the algorithm for a selected
type of data, by providing it with a few samples. The result of the training is stored
in a file selected with the `-o` option (default name is `dictionary`),
which can be loaded before compression and decompression.

Using a dictionary, the compression ratio achievable on small data improves dramatically.
These compression gains are achieved while simultaneously providing faster compression and decompression speeds.
Dictionary work if there is some correlation in a family of small data (there is no universal dictionary).
Hence, deploying one dictionary per type of data will provide the greater benefits.
Dictionary gains are mostly effective in the first few KB. Then, the compression algorithm
will rely more and more on previously decoded content to compress the rest of the file.

Usage of the dictionary builder and created dictionaries with CLI:

1. Create the dictionary : `zstd --train FullPathToTrainingSet/* -o dictionaryName`
2. Compress with the dictionary: `zstd FILE -D dictionaryName`
3. Decompress with the dictionary: `zstd --decompress FILE.zst -D dictionaryName`


#### Benchmark in Command Line Interface
CLI includes in-memory compression benchmark module for zstd.
The benchmark is conducted using given filenames. The files are read into memory and joined together.
It makes benchmark more precise as it eliminates I/O overhead.
Many filenames can be supplied as multiple parameters, parameters with wildcards or
names of directories can be used as parameters with the `-r` option.

The benchmark measures ratio, compressed size, compression and decompression speed.
One can select compression levels starting from `-b` and ending with `-e`.
The `-i` parameter selects minimal time used for each of tested levels.


#### Usage of Command Line Interface
The full list of options can be obtained with `-h` or `-H` parameter:
```
Usage :
      zstd [args] [FILE(s)] [-o file]

FILE    : a filename
          with no FILE, or when FILE is - , read standard input
Arguments :
 -#     : # compression level (1-19, default:3)
 -d     : decompression
 -D file: use `file` as Dictionary
 -o file: result stored into `file` (only if 1 input file)
 -f     : overwrite output without prompting and (de)compress links
--rm    : remove source file(s) after successful de/compression
 -k     : preserve source file(s) (default)
 -h/-H  : display help/long help and exit

Advanced arguments :
 -V     : display Version number and exit
 -v     : verbose mode; specify multiple times to increase verbosity
 -q     : suppress warnings; specify twice to suppress errors too
 -c     : force write to standard output, even if it is the console
--ultra : enable levels beyond 19, up to 22 (requires more memory)
 -T#    : use # threads for compression (default:1)
 -B#    : select size of each job (default:0==automatic)
--no-dictID : don't write dictID into header (dictionary compression)
--[no-]check : integrity check (default:enabled)
 -r     : operate recursively on directories
--format=gzip : compress files to the .gz format
--test  : test compressed file integrity
--[no-]sparse : sparse mode (default:disabled)
 -M#    : Set a memory usage limit for decompression
--      : All arguments after "--" are treated as files

Dictionary builder :
--train ## : create a dictionary from a training set of files
--train-cover[=k=#,d=#,steps=#] : use the cover algorithm with optional args
--train-legacy[=s=#] : use the legacy algorithm with selectivity (default: 9)
 -o file : `file` is dictionary name (default: dictionary)
--maxdict=# : limit dictionary to specified size (default : 112640)
--dictID=# : force dictionary ID to specified value (default: random)

Benchmark arguments :
 -b#    : benchmark file(s), using # compression level (default : 1)
 -e#    : test all compression levels from -bX to # (default: 1)
 -i#    : minimum evaluation time in seconds (default : 3s)
 -B#    : cut file into independent blocks of size # (default: no block)
--priority=rt : set process priority to real-time
```
