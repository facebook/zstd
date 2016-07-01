Zstandard Compression Format Description
========================================

### Notices

Copyright (c) 2016 Yann Collet

Permission is granted to copy and distribute this document
for any  purpose and without charge,
including translations into other  languages
and incorporation into compilations,
provided that the copyright notice and this notice are preserved,
and that any substantive changes or deletions from the original
are clearly marked.
Distribution of this document is unlimited.

### Version

0.1.0 (30/06/2016 - unfinished)


Introduction
------------

The purpose of this document is to define a lossless compressed data format,
that is independent of CPU type, operating system,
file system and character set, suitable for
File compression, Pipe and streaming compression
using the [Zstandard algorithm](http://www.zstandard.org).

The data can be produced or consumed,
even for an arbitrarily long sequentially presented input data stream,
using only an a priori bounded amount of intermediate storage,
and hence can be used in data communications.
The format uses the Zstandard compression method,
and optional [xxHash-64 checksum method](http://www.xxhash.org),
for detection of data corruption.

The data format defined by this specification
does not attempt to allow random access to compressed data.

This specification is intended for use by implementers of software
to compress data into Zstandard format and/or decompress data from Zstandard format.
The text of the specification assumes a basic background in programming
at the level of bits and other primitive data representations.

Unless otherwise indicated below,
a compliant compressor must produce data sets
that conform to the specifications presented here.
It doesn’t need to support all options though.

A compliant decompressor must be able to decompress
at least one working set of parameters
that conforms to the specifications presented here.
It may also ignore informative fields, such as checksum.
Whenever it does not support a parameter defined in the compressed stream,
it must produce a non-ambiguous error code and associated error message
explaining which parameter is unsupported.


Definitions
-----------
A content compressed by Zstandard is transformed into a Zstandard __frame__.
Multiple frames can be appended into a single file or stream.
A frame is totally independent, has a defined beginning and end,
and a set of parameters which tells the decoder how to decompress it.

A frame encapsulates one or multiple __blocks__.
Each block can be compressed or not,
and has a guaranteed maximum content size, which depends on frame parameters.
Unlike frames, each block depends on previous blocks for proper decoding.
However, each block can be decompressed without waiting for its successor,
allowing streaming operations.


General Structure of Zstandard Frame format
-------------------------------------------

| MagicNb |  F. Header | Block | (More blocks) | EndMark |
|:-------:|:----------:| ----- | ------------- | ------- |
| 4 bytes | 2-14 bytes |       |               | 3 bytes |

__Magic Number__

4 Bytes, Little endian format.
Value : 0xFD2FB527

__Frame Header__

2 to 14 Bytes, to be detailed in the next part.

__Data Blocks__

To be detailed later on.
That’s where compressed data is stored.

__EndMark__

The flow of blocks ends when the last block header brings an _end signal_ .
This last block header may optionally host a __Content Checksum__ .

__Content Checksum__

Content Checksum verify that frame content has been regenrated correctly.
The content checksum is the result
of [xxh64() hash function](https://www.xxHash.com)
digesting the original (decoded) data as input, and a seed of zero.
Bits from 11 to 32 (included) are extracted to form a 22 bits checksum
stored into the last block header.
```
contentChecksum = (XXH64(content, size, 0) >> 11) & (1<<22)-1);
```
Content checksum is only present when its associated flag
is set in the frame descriptor.
Its usage is optional.

__Frame Concatenation__

In some circumstances, it may be required to append multiple frames,
for example in order to add new data to an existing compressed file
without re-framing it.

In such case, each frame brings its own set of descriptor flags.
Each frame is considered independent.
The only relation between frames is their sequential order.

The ability to decode multiple concatenated frames
within a single stream or file is left outside of this specification.
As an example, the reference `zstd` command line utility is able
to decode all concatenated frames in their sequential order,
delivering the final decompressed result as if it was a single content.


Frame Header
-------------

| FHD     | (WD)      | (Content Size) | (dictID)  |
| ------- | --------- |:--------------:| --------- |
| 1 byte  | 0-1 byte  |  0 - 8 bytes   | 0-4 bytes |

Frame header has a variable size, which uses a minimum of 2 bytes,
and up to 14 bytes depending on optional parameters.

__FHD byte__ (Frame Header Descriptor)

The first Header's byte is called the Frame Header Descriptor.
It tells which other fields are present.
Decoding this byte is enough to get the full size of the Frame Header.

|  BitNb  |   7-6  |    5    |   4    |    3     |    2     |    1-0   |
| ------- | ------ | ------- | ------ | -------- | -------- | -------- |
|FieldName| FCSize | Segment | Unused | Reserved | Checksum |  dictID  |

In this table, bit 7 is highest bit, while bit 0 is lowest.

__Frame Content Size flag__

This is a 2-bits flag (`= FHD >> 6`),
specifying if decompressed data size is provided within the header.

|  Value  |  0  |  1  |  2  |  3  |
| ------- | --- | --- | --- | --- |
|FieldSize| 0-1 |  2  |  4  |  8  |

Value 0 has a double meaning :
it either means `0` (size not provided) _if_ the `WD` byte is present,
or it means `1` byte (size <= 255 bytes).

__Single Segment__

If this flag is set,
data shall be regenerated within a single continuous memory segment.
In which case, `WD` byte __is not present__,
but `Frame Content Size` field necessarily is.

As a consequence, the decoder must allocate a memory segment
of size `>= Frame Content Size`.

In order to preserve the decoder from unreasonable memory requirement,
a decoder can refuse a compressed frame
which requests a memory size beyond decoder's authorized range.

For broader compatibility, decoders are recommended to support
memory sizes of 8 MB at least.
However, this is merely a recommendation,
and each decoder is free to support higher or lower limits,
depending on local limitations.

__Unused bit__

The value of this bit is unimportant
and not interpreted by a decoder compliant with this specification version.
It may be used in a future revision,
to signal a property which is not required to properly decode the frame.

__Reserved bit__

This bit is reserved for some future feature.
Its value _must be zero_.
A decoder compliant with this specification version must ensure it is not set.
This bit may be used in a future revision,
to signal a feature that must be interpreted in order to decode the frame.

__Content checksum flag__

If this flag is set, a content checksum will be present into the EndMark.
The checksum is a 22 bits value extracted from the XXH64() of data.
See __Content Checksum__ .

__Dictionary ID flag__

This is a 2-bits flag (`= FHD & 3`),
telling if a dictionary ID is provided within the header

|  Value  |  0  |  1  |  2  |  3  |
| ------- | --- | --- | --- | --- |
|FieldSize|  0  |  1  |  2  |  4  |

__WD byte__ (Window Descriptor)

Provides guarantees on maximum back-reference distance
that will be present within compressed data.
This information is useful for decoders to allocate enough memory.

|   BitNb   |    7-3   |    0-2   |
| --------- | -------- | -------- |
| FieldName | Exponent | Mantissa |

Maximum distance is given by the following formulae :
```
windowLog = 10 + Exponent;
windowBase = 1 << windowLog;
windowAdd = (windowBase / 8) * Mantissa;
windowSize = windowBase + windowAdd;
```
The minimum window size is 1 KB.
The maximum size is (15*(2^38))-1 bytes, which is almost 1.875 TB.

To properly decode compressed data,
a decoder will need to allocate a buffer of at least `windowSize` bytes.

Note that `WD` byte is optional. It's not present in `single segment` mode.
In which case, the maximum back-reference distance is the content size itself,
which can be any value from 1 to 2^64-1 bytes (16 EB).

In order to preserve decoder from unreasonable memory requirements,
a decoder can refuse a compressed frame
which requests a memory size beyond decoder's authorized range.

For better interoperability,
decoders are recommended to be compatible with window sizes of 8 MB.
Encoders are recommended to not request more than 8 MB.
It's merely a recommendation though,
decoders are free to support larger or lower limits,
depending on local limitations.

__Frame Content Size__

This is the original (uncompressed) size.
This information is optional, and only present if associated flag is set.
Content size is provided using 1, 2, 4 or 8 Bytes.
Format is Little endian.

| Field Size |    Range   |
| ---------- | ---------- |
|     0      |      0     |
|     1      |   0 - 255  |
|     2      | 256 - 65791|
|     4      | 0 - 2^32-1 |
|     8      | 0 - 2^64-1 |

When field size is 1, 4 or 8 bytes, the value is read directly.
When field size is 2, _an offset of 256 is added_.
It's allowed to represent a small size (ex: `18`) using the 8-bytes variant.
A size of `0` means `content size is unknown`.
In which case, the `WD` byte will necessarily be present,
and becomes the only hint to determine memory allocation.

In order to preserve decoder from unreasonable memory requirement,
a decoder can refuse a compressed frame
which requests a memory size beyond decoder's authorized range.

__Dictionary ID__

This is a variable size field, which contains a single ID.
It checks if the correct dictionary is used for decoding.
Note that this field is optional. If it's not present,
it's up to the caller to make sure it uses the correct dictionary.

Field size depends on __Dictionary ID flag__.
1 byte can represent an ID 0-255.
2 bytes can represent an ID 0-65535.
4 bytes can represent an ID 0-(2^32-1).

It's allowed to represent a small ID (for example `13`)
with a large 4-bytes dictionary ID, losing some efficiency in the process.


Data Blocks
-----------

| B. Header |  data  |
|:---------:| ------ |
|  3 bytes  |        |


__Block Header__

This field uses 3-bytes, format is __big-endian__.

The 2 highest bits represent the `block type`,
while the remaining 22 bits represent the (compressed) block size.

There are 4 block types :

|    Value   |      0     |  1  |  2  |    3    |
| ---------- | ---------- | --- | --- | ------- |
| Block Type | Compressed | Raw | RLE | EndMark |

- Compressed : this is a Zstandard compressed block,
  detailed in a later part of this specification.
  "block size" is the compressed size.
  Decompressed size is unknown,
  but its maximum possible value is guaranteed (see below)
- Raw : this is an uncompressed block.
  "block size" is the number of bytes to read and copy.
- RLE : this is a single byte, repeated N times.
  In which case, "block size" is the size to regenerate,
  while the "compressed" block is just 1 byte (the byte to repeat).
- EndMark : this is not a block. Signal the end of the frame.
  The rest of the field may be optionally filled by a checksum
  (see frame checksum).

Block sizes must respect a few rules :
- In compressed mode, compressed size if always strictly `< contentSize`.
- Block decompressed size is necessarily <= maximum back-reference distance .
- Block decompressed size is necessarily <= 128 KB


__Data__

Where the actual data to decode stands.
It might be compressed or not, depending on previous field indications.
A data block is not necessarily "full" :
since an arbitrary “flush” may happen anytime,
block content can be any size, up to Block Maximum Size.
Block Maximum Size is the smallest of :
- Max back-reference distance
- 128 KB


Skippable Frames
----------------

| Magic Number | Frame Size | User Data |
|:------------:|:----------:| --------- |
|   4 bytes    |  4 bytes   |           |

Skippable frames allow the insertion of user-defined data
into a flow of concatenated frames.
Its design is pretty straightforward,
with the sole objective to allow the decoder to quickly skip
over user-defined data and continue decoding.

Skippable frames defined in this specification are compatible with LZ4 ones.


__Magic Number__ :

4 Bytes, Little endian format.
Value : 0x184D2A5X, which means any value from 0x184D2A50 to 0x184D2A5F.
All 16 values are valid to identify a skippable frame.

__Frame Size__ :

This is the size, in bytes, of the following User Data
(without including the magic number nor the size field itself).
4 Bytes, Little endian format, unsigned 32-bits.
This means User Data can’t be bigger than (2^32-1) Bytes.

__User Data__ :

User Data can be anything. Data will just be skipped by the decoder.


Compressed block format
-----------------------
This specification details the content of a _compressed block_.
A compressed block has a size, which must be known in order to decode it.
It also has a guaranteed maximum regenerated size,
in order to properly allocate destination buffer.
See "Frame format" for more details.

A compressed block consists of 2 sections :
- Literals section
- Sequences section

### Compressed Literals

Literals are compressed using order-0 huffman compression.
During sequence phase, literals will be entangled with match copy operations.
All literals are regrouped in the first part of the block.
They can be decoded first, and then copied during sequence operations,
or they can be decoded on the flow, as needed by sequences.

| Header | (Tree Description) | Stream1 | (Stream2) | (Stream3) | (Stream4) |
| ------ | ------------------ | ------- | --------- | --------- | --------- |

Literals can be compressed, or uncompressed.
When compressed, an optional tree description can be present,
followed by 1 or 4 streams.

#### Block Literal Header

Header is in charge of describing precisely how literals are packed.
It's a byte-aligned variable-size bitfield, ranging from 1 to 5 bytes,
using big-endian convention.

| BlockType | sizes format | (compressed size) | regenerated size |
| --------- | ------------ | ----------------- | ---------------- |
|   2 bits  |  1 - 2 bits  |    0 - 18 bits    |    5 - 20 bits   |

__Block Type__ :

This is a 2-bits field, describing 4 different block types :

|    Value   |      0     |    1   |  2  |    3    |
| ---------- | ---------- | ------ | --- | ------- |
| Block Type | Compressed | Repeat | Raw |   RLE   |

- Compressed : This is a standard huffman-compressed block,
               starting with a huffman tree description.
               See details below.
- Repeat Stats : This is a huffman-compressed block,
               using huffman tree from previous huffman-compressed block.
               Huffman tree description will be skipped.
               Compressed stream is equivalent to "compressed" block type.
- Raw : Literals are stored uncompressed.
- RLE : Literals consist of a single byte value repeated N times.

__Sizes format__ :

Sizes format are divided into 2 families :

- For compressed block, it requires to decode both the compressed size
  and the decompressed size. It will also decode the number of streams.
- For Raw or RLE blocks, it's enough to decode the size to regenerate.

For values spanning several bytes, convention is Big-endian.

__Sizes format for Raw or RLE block__ :

- Value : 0x : Regenerated size uses 5 bits (0-31).
               Total literal header size is 1 byte.
               `size = h[0] & 31;`
- Value : 10 : Regenerated size uses 12 bits (0-4095).
               Total literal header size is 2 bytes.
               `size = ((h[0] & 15) << 8) + h[1];`
- Value : 11 : Regenerated size uses 20 bits (0-1048575).
               Total literal header size is 2 bytes.
               `size = ((h[0] & 15) << 16) + (h[1]<<8) + h[2];`

Note : it's allowed to represent a short value (ex : `13`)
using a long format, accepting the reduced compacity.

__Sizes format for Compressed Block__ :

Note : also applicable to "repeat-stats" blocks.
- Value : 00 : 4 streams
               Compressed and regenerated sizes use 10 bits (0-1023)
               Total literal header size is 3 bytes
- Value : 01 : _Single stream_
               Compressed and regenerated sizes use 10 bits (0-1023)
               Total literal header size is 3 bytes
- Value : 10 : 4 streams
               Compressed and regenerated sizes use 14 bits (0-16383)
               Total literal header size is 4 bytes
- Value : 10 : 4 streams
               Compressed and regenerated sizes use 18 bits (0-262143)
               Total literal header size is 5 bytes



Version changes
---------------
0.1 : initial release
