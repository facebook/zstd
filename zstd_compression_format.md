Zstandard Compression Format
============================

### Notices

Copyright (c) 2016 Yann Collet

Permission is granted to copy and distribute this document
for any purpose and without charge,
including translations into other languages
and incorporation into compilations,
provided that the copyright notice and this notice are preserved,
and that any substantive changes or deletions from the original
are clearly marked.
Distribution of this document is unlimited.

### Version

0.2.0 (22/07/16)


Introduction
------------

The purpose of this document is to define a lossless compressed data format,
that is independent of CPU type, operating system,
file system and character set, suitable for
file compression, pipe and streaming compression,
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


Overall conventions
-----------
In this document square brackets i.e. `[` and `]` are used to indicate optional fields or parameters.


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


Frame Concatenation
-------------------

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


General Structure of Zstandard Frame format
-------------------------------------------
The structure of a single Zstandard frame is following:

| `Magic_Number` | `Frame_Header` |`Data_Block`| [More data blocks] |`End_Marker`|
|:--------------:|:--------------:|:----------:| ------------------ |:----------:|
| 4 bytes        |  2-14 bytes    | n bytes    |                    | 3 bytes    |

__`Magic_Number`__

4 Bytes, Little endian format.
Value : 0xFD2FB527

__`Frame_Header`__

2 to 14 Bytes, detailed in [next part](#the-structure-of-frame_header).

__`Data_Block`__

Detailed in [next chapter](#the-structure-of-data_block).
That’s where compressed data is stored.

__`End_Marker`__

The flow of blocks ends when the last block header brings an _end signal_.
This last block header may optionally host a `Content_Checksum`.

##### __`Content_Checksum`__

`Content_Checksum` allow to verify that frame content has been regenerated correctly.
The content checksum is the result
of [xxh64() hash function](https://www.xxHash.com)
digesting the original (decoded) data as input, and a seed of zero.
Bits from 11 to 32 (included) are extracted to form a 22 bits checksum
stored within `End_Marker`.
```
mask22bits = (1<<22)-1;
contentChecksum = (XXH64(content, size, 0) >> 11) & mask22bits;
```
`Content_Checksum` is only present when its associated flag
is set in the frame descriptor.
Its usage is optional.



The structure of `Frame_Header`
-------------------------------
The `Frame_Header` has a variable size, which uses a minimum of 2 bytes,
and up to 14 bytes depending on optional parameters.
The structure of `Frame_Header` is following:

| `Frame_Header_Descriptor` | [`Window_Descriptor`] | [`Dictionary_ID`] | [`Frame_Content_Size`] |
| ------------------------- | --------------------- | ----------------- | ---------------------- |
| 1 byte                    | 0-1 byte              | 0-4 bytes         | 0-8 bytes              |

### `Frame_Header_Descriptor`

The first header's byte is called the `Frame_Header_Descriptor`.
It tells which other fields are present.
Decoding this byte is enough to tell the size of `Frame_Header`.

| Bit number | Field name                |
| ---------- | ----------                |
| 7-6        | `Frame_Content_Size_flag` |
| 5          | `Single_Segment_flag`     |
| 4          | `Unused_bit`              |
| 3          | `Reserved_bit`            |
| 2          | `Content_Checksum_flag`   |
| 1-0        | `Dictionary_ID_flag`      |

In this table, bit 7 is highest bit, while bit 0 is lowest.

__`Single_Segment_flag`__

If `Single_Segment_flag` is not set then `Window_Descriptor` is mandatory and `Frame_Content_Size_flag` will be ignored.

If `Single_Segment_flag` is set then `Window_Descriptor` should be absent and `Frame_Content_Size_flag` will be used along with a mandatory `Frame_Content_Size` field.
As a consequence, the decoder must allocate a single continuous memory segment of size equal or bigger than `Frame_Content_Size`.

In order to preserve the decoder from unreasonable memory requirement,
a decoder can reject a compressed frame
which requests a memory size beyond decoder's authorized range.

For broader compatibility, decoders are recommended to support
memory sizes of at least 8 MB.
This is just a recommendation,
each decoder is free to support higher or lower limits,
depending on local limitations.

__`Frame_Content_Size_flag`__

This is a 2-bits flag (`= FHD >> 6`) used only if `Single_Segment_flag` is set.
In this case Value can be converted to Field size that is number of bytes used by `Frame_Content_Size` according to the following table:

|  Value   |  0  |  1  |  2  |  3  |
|----------| --- | --- | --- | --- |
|Field size|  1  |  2  |  4  |  8  |

__`Unused_bit`__

The value of this bit should be set to zero.
A decoder compliant with this specification version should not interpret it.
It might be used in a future version,
to signal a property which is not mandatory to properly decode the frame.

__`Reserved_bit`__

This bit is reserved for some future feature.
Its value _must be zero_.
A decoder compliant with this specification version must ensure it is not set.
This bit may be used in a future revision,
to signal a feature that must be interpreted in order to decode the frame.

__`Content_Checksum_flag`__

If this flag is set, a content checksum will be present within `End_Marker`.
The checksum is a 22 bits value extracted from the XXH64() of data,
and stored within `End_Marker`. See [`Content_Checksum`](#content_checksum) .

__`Dictionary_ID_flag`__

This is a 2-bits flag (`= FHD & 3`),
telling if a dictionary ID is provided within the header.
It also specifies the size of this field.

|  Value   |  0  |  1  |  2  |  3  |
| -------- | --- | --- | --- | --- |
|Field size|  0  |  1  |  2  |  4  |

### `Window_Descriptor`

Provides guarantees on maximum back-reference distance
that will be present within compressed data.
This information is useful for decoders to allocate enough memory.

The `Window_Descriptor` byte is optional. It should be absent if `Single_Segment_flag` is set.
In this case, the maximum back-reference distance is the content size itself,
which can be any value from 1 to 2^64-1 bytes (16 EB).

| Bit numbers |    7-3   |    0-2   |
| ----------- | -------- | -------- |
| Field name  | Exponent | Mantissa |

Maximum distance is given by the following formulae :
```
windowLog = 10 + Exponent;
windowBase = 1 << windowLog;
windowAdd = (windowBase / 8) * Mantissa;
windowSize = windowBase + windowAdd;
```
The minimum window size is 1 KB.
The maximum size is `15*(1<<38)` bytes, which is 1.875 TB.

To properly decode compressed data,
a decoder will need to allocate a buffer of at least `windowSize` bytes.

In order to preserve decoder from unreasonable memory requirements,
a decoder can refuse a compressed frame
which requests a memory size beyond decoder's authorized range.

For improved interoperability,
decoders are recommended to be compatible with window sizes of 8 MB.
Encoders are recommended to not request more than 8 MB.
It's merely a recommendation though,
decoders are free to support larger or lower limits,
depending on local limitations.

### `Dictionary_ID`

This is a variable size field, which contains
the ID of the dictionary required to properly decode the frame.
Note that this field is optional. When it's not present,
it's up to the caller to make sure it uses the correct dictionary.

Field size depends on `Dictionary_ID_flag`.
1 byte can represent an ID 0-255.
2 bytes can represent an ID 0-65535.
4 bytes can represent an ID 0-4294967295.

It's allowed to represent a small ID (for example `13`)
with a large 4-bytes dictionary ID, losing some compacity in the process.

_Reserved ranges :_
If the frame is going to be distributed in a private environment,
any dictionary ID can be used.
However, for public distribution of compressed frames using a dictionary,
the following ranges are reserved for future use and should not be used :
- low range : 1 - 32767
- high range : >= (2^31)


### `Frame_Content_Size`

This is the original (uncompressed) size.
This information is optional, and only present if `Single_Segment_flag` is set.
Content size is provided using 1, 2, 4 or 8 bytes according to `Frame_Content_Size_flag`.
Format is Little endian.

| Field Size |    Range   |
| ---------- | ---------- |
|     1      |   0 - 255  |
|     2      | 256 - 65791|
|     4      | 0 - 2^32-1 |
|     8      | 0 - 2^64-1 |

When field size is 1, 4 or 8 bytes, the value is read directly.
When field size is 2, _the offset of 256 is added_.
It's allowed to represent a small size (for example `18`) using any compatible variant.

In order to preserve decoder from unreasonable memory requirement,
a decoder can refuse a compressed frame
which requests a memory size beyond decoder's authorized range.


The structure of `Data_Block`
-----------------------------
The structure of `Data_Block` is following:

| `Block_Type` | `Block_Size` | `Block_Content` |
|:------------:|:------------:|:---------------:|
|  2 bits      |  22 bits     |  n bytes        |

__`Block_Type` and `Block_Size`__

The block header uses 3-bytes, format is __little-endian__.
The 2 highest bits represent the `Block_Type`,
while the remaining 22 bits represent the (compressed) `Block_Size`.

There are 4 block types :

|    Value     |      0      |     1       |  2                 |    3      |
| ------------ | ----------- | ----------- | ------------------ | --------- |
| `Block_Type` | `Raw_Block` | `RLE_Block` | `Compressed_Block` | `EndMark` |

- `Raw_Block` - this is an uncompressed block.
  `Block_Size` is the number of bytes to read and copy.
- `RLE_Block` - this is a single byte, repeated N times.
  In which case, `Block_Size` is the size to regenerate,
  while the "compressed" block is just 1 byte (the byte to repeat).
- `Compressed_Block` - this is a [Zstandard compressed block](#the-format-of-compressed_block),
  detailed in another section of this specification.
  `Block_Size` is the compressed size.
  Decompressed size is unknown,
  but its maximum possible value is guaranteed (see below)
- `EndMark` - this is not a block. It signals the end of the frame.
  The rest of the field may be optionally filled by a checksum
  (see [`Content_Checksum`](#content_checksum)).

Block sizes must respect a few rules :
- In compressed mode, compressed size if always strictly `< decompressed size`.
- Block decompressed size is always <= maximum back-reference distance .
- Block decompressed size is always <= 128 KB


__`Block_Content`__

The `Block_Content` is where the actual data to decode stands.
It might be compressed or not, depending on previous field indications.
A data block is not necessarily "full" :
since an arbitrary “flush” may happen anytime,
block decompressed content can be any size,
up to `Block_Maximum_Decompressed_Size`, which is the smallest of :
- Maximum back-reference distance
- 128 KB


Skippable Frames
----------------

| `Magic_Number` | `Frame_Size` | `User_Data` |
|:--------------:|:------------:|:-----------:|
|   4 bytes      |  4 bytes     |   n bytes   |

Skippable frames allow the insertion of user-defined data
into a flow of concatenated frames.
Its design is pretty straightforward,
with the sole objective to allow the decoder to quickly skip
over user-defined data and continue decoding.

Skippable frames defined in this specification are compatible with [LZ4] ones.

[LZ4]:http://www.lz4.org

__`Magic_Number`__

4 Bytes, Little endian format.
Value : 0x184D2A5X, which means any value from 0x184D2A50 to 0x184D2A5F.
All 16 values are valid to identify a skippable frame.

__`Frame_Size`__

This is the size, in bytes, of the following `User_Data`
(without including the magic number nor the size field itself).
This field is represented using 4 Bytes, Little endian format, unsigned 32-bits.
This means `User_Data` can’t be bigger than (2^32-1) bytes.

__`User_Data`__

The `User_Data` can be anything. Data will just be skipped by the decoder.


The format of `Compressed_Block`
--------------------------------
The size of `Compressed_Block` must be provided using `Block_Size` field from `Data_Block`.
The `Compressed_Block` has a guaranteed maximum regenerated size,
in order to properly allocate destination buffer.
See [`Data_Block`](#the-structure-of-data_block) for more details.

A compressed block consists of 2 sections :
- [Literals section](#literals-section)
- [Sequences section](#sequences-section)

### Prerequisites
To decode a compressed block, the following elements are necessary :
- Previous decoded blocks, up to a distance of `windowSize`,
  or all previous blocks when `Single_Segment_flag` is set.
- List of "recent offsets" from previous compressed block.
- Decoding tables of previous compressed block for each symbol type
  (literals, litLength, matchLength, offset).


### Literals section

During sequence phase, literals will be entangled with match copy operations.
All literals are regrouped in the first part of the block.
They can be decoded first, and then copied during sequence operations,
or they can be decoded on the flow, as needed by sequence commands.

| Literals section header | [Huffman Tree Description] | Stream1 | [Stream2] | [Stream3] | [Stream4] |
| ----------------------- | -------------------------- | ------- | --------- | --------- | --------- |

Literals can be stored uncompressed or compressed using Huffman prefix codes.
When compressed, an optional tree description can be present,
followed by 1 or 4 streams.


#### Literals section header

Header is in charge of describing how literals are packed.
It's a byte-aligned variable-size bitfield, ranging from 1 to 5 bytes,
using little-endian convention.

| Literals Block Type | sizes format | regenerated size | [compressed size] |
| ------------------- | ------------ | ---------------- | ----------------- |
|   2 bits            |  1 - 2 bits  |    5 - 20 bits   |    0 - 18 bits    |

In this representation, bits on the left are smallest bits.

__Literals Block Type__ :

This field uses 2 lowest bits of first byte, describing 4 different block types :

|       Value         |  0  |  1  |      2     |      3      |
| ------------------- | --- | --- | ---------- | ----------- |
| Literals Block Type | Raw | RLE | Compressed | RepeatStats |    

- Raw literals block - Literals are stored uncompressed.
- RLE literals block - Literals consist of a single byte value repeated N times.
- Compressed literals block - This is a standard huffman-compressed block,
        starting with a huffman tree description.
        See details below.
- Repeat Stats literals block - This is a huffman-compressed block,
        using huffman tree _from previous huffman-compressed literals block_.
        Huffman tree description will be skipped.

__Sizes format__ :

Sizes format are divided into 2 families :

- For compressed block, it requires to decode both the compressed size
  and the decompressed size. It will also decode the number of streams.
- For Raw or RLE blocks, it's enough to decode the size to regenerate.

For values spanning several bytes, convention is Little-endian.

__Sizes format for Raw and RLE literals block__ :

- Value : x0 : Regenerated size uses 5 bits (0-31).
               Total literal header size is 1 byte.
               `size = h[0]>>3;`
- Value : 01 : Regenerated size uses 12 bits (0-4095).
               Total literal header size is 2 bytes.
               `size = (h[0]>>4) + (h[1]<<4);`
- Value : 11 : Regenerated size uses 20 bits (0-1048575).
               Total literal header size is 3 bytes.
               `size = (h[0]>>4) + (h[1]<<4) + (h[2]<<12);`

Note : it's allowed to represent a short value (ex : `13`)
using a long format, accepting the reduced compacity.

__Sizes format for Compressed literals block and Repeat Stats literals block__ :

- Value : 00 : _Single stream_.
               Compressed and regenerated sizes use 10 bits (0-1023).
               Total literal header size is 3 bytes.
- Value : 01 : 4 streams.
               Compressed and regenerated sizes use 10 bits (0-1023).
               Total literal header size is 3 bytes.
- Value : 10 : 4 streams.
               Compressed and regenerated sizes use 14 bits (0-16383).
               Total literal header size is 4 bytes.
- Value : 11 : 4 streams.
               Compressed and regenerated sizes use 18 bits (0-262143).
               Total literal header size is 5 bytes.

Compressed and regenerated size fields follow little-endian convention.

#### Huffman Tree description

This section is only present when literals block type is `Compressed` (`0`).

Prefix coding represents symbols from an a priori known alphabet
by bit sequences (codewords), one codeword for each symbol,
in a manner such that different symbols may be represented
by bit sequences of different lengths,
but a parser can always parse an encoded string
unambiguously symbol-by-symbol.

Given an alphabet with known symbol frequencies,
the Huffman algorithm allows the construction of an optimal prefix code
using the fewest bits of any possible prefix codes for that alphabet.

Prefix code must not exceed a maximum code length.
More bits improve accuracy but cost more header size,
and require more memory or more complex decoding operations.
This specification limits maximum code length to 11 bits.


##### Representation

All literal values from zero (included) to last present one (excluded)
are represented by `weight` values, from 0 to `maxBits`.
Transformation from `weight` to `nbBits` follows this formulae :
`nbBits = weight ? maxBits + 1 - weight : 0;` .
The last symbol's weight is deduced from previously decoded ones,
by completing to the nearest power of 2.
This power of 2 gives `maxBits`, the depth of the current tree.

__Example__ :
Let's presume the following huffman tree must be described :

| literal |  0  |  1  |  2  |  3  |  4  |  5  |
| ------- | --- | --- | --- | --- | --- | --- |
| nbBits  |  1  |  2  |  3  |  0  |  4  |  4  |

The tree depth is 4, since its smallest element uses 4 bits.
Value `5` will not be listed, nor will values above `5`.
Values from `0` to `4` will be listed using `weight` instead of `nbBits`.
Weight formula is : `weight = nbBits ? maxBits + 1 - nbBits : 0;`
It gives the following serie of weights :

| weights |  4  |  3  |  2  |  0  |  1  |
| ------- | --- | --- | --- | --- | --- |
| literal |  0  |  1  |  2  |  3  |  4  |

The decoder will do the inverse operation :
having collected weights of literals from `0` to `4`,
it knows the last literal, `5`, is present with a non-zero weight.
The weight of `5` can be deducted by joining to the nearest power of 2.
Sum of 2^(weight-1) (excluding 0) is :
`8 + 4 + 2 + 0 + 1 = 15`
Nearest power of 2 is 16.
Therefore, `maxBits = 4` and `weight[5] = 1`.

##### Huffman Tree header

This is a single byte value (0-255),
which tells how to decode the list of weights.

- if headerByte >= 128 : this is a direct representation,
  where each weight is written directly as a 4 bits field (0-15).
  The full representation occupies `((nbSymbols+1)/2)` bytes,
  meaning it uses a last full byte even if nbSymbols is odd.
  `nbSymbols = headerByte - 127;`.
  Note that maximum nbSymbols is 255-127 = 128.
  A larger serie must necessarily use FSE compression.

- if headerByte < 128 :
  the serie of weights is compressed by FSE.
  The length of the FSE-compressed serie is `headerByte` (0-127).

##### FSE (Finite State Entropy) compression of huffman weights

The serie of weights is compressed using FSE compression.
It's a single bitstream with 2 interleaved states,
sharing a single distribution table.

To decode an FSE bitstream, it is necessary to know its compressed size.
Compressed size is provided by `headerByte`.
It's also necessary to know its _maximum possible_ decompressed size,
which is `255`, since literal values span from `0` to `255`,
and last symbol value is not represented.

An FSE bitstream starts by a header, describing probabilities distribution.
It will create a Decoding Table.
Table must be pre-allocated, which requires to support a maximum accuracy.
For a list of huffman weights, maximum accuracy is 7 bits.

FSE header is [described in relevant chapter](#fse-distribution-table--condensed-format),
and so is [FSE bitstream](#bitstream).
The main difference is that Huffman header compression uses 2 states,
which share the same FSE distribution table.
Bitstream contains only FSE symbols (no interleaved "raw bitfields").
The number of symbols to decode is discovered
by tracking bitStream overflow condition.
When both states have overflowed the bitstream, end is reached.


##### Conversion from weights to huffman prefix codes

All present symbols shall now have a `weight` value.
It is possible to transform weights into nbBits, using this formula :
`nbBits = nbBits ? maxBits + 1 - weight : 0;` .

Symbols are sorted by weight. Within same weight, symbols keep natural order.
Symbols with a weight of zero are removed.
Then, starting from lowest weight, prefix codes are distributed in order.

__Example__ :
Let's presume the following list of weights has been decoded :

| Literal |  0  |  1  |  2  |  3  |  4  |  5  |
| ------- | --- | --- | --- | --- | --- | --- |
|  weight |  4  |  3  |  2  |  0  |  1  |  1  |

Sorted by weight and then natural order,
it gives the following distribution :

| Literal      |  3  |  4  |  5  |  2  |  1  |   0  |
| ------------ | --- | --- | --- | --- | --- | ---- |
| weight       |  0  |  1  |  1  |  2  |  3  |   4  |
| nb bits      |  0  |  4  |  4  |  3  |  2  |   1  |
| prefix codes | N/A | 0000| 0001| 001 | 01  |   1  |


#### Literals bitstreams

##### Bitstreams sizes

As seen in a previous paragraph,
there are 2 flavors of huffman-compressed literals :
single stream, and 4-streams.

4-streams is useful for CPU with multiple execution units and OoO operations.
Since each stream can be decoded independently,
it's possible to decode them up to 4x faster than a single stream,
presuming the CPU has enough parallelism available.

For single stream, header provides both the compressed and regenerated size.
For 4-streams though,
header only provides compressed and regenerated size of all 4 streams combined.
In order to properly decode the 4 streams,
it's necessary to know the compressed and regenerated size of each stream.

Regenerated size of each stream can be calculated by `(totalSize+3)/4`,
except for last one, which can be up to 3 bytes smaller, to reach `totalSize`.

Compressed size is provided explicitly : in the 4-streams variant,
bitstreams are preceded by 3 unsigned Little Endian 16-bits values.
Each value represents the compressed size of one stream, in order.
The last stream size is deducted from total compressed size
and from previously decoded stream sizes :
`stream4CSize = totalCSize - 6 - stream1CSize - stream2CSize - stream3CSize;`

##### Bitstreams read and decode

Each bitstream must be read _backward_,
that is starting from the end down to the beginning.
Therefore it's necessary to know the size of each bitstream.

It's also necessary to know exactly which _bit_ is the latest.
This is detected by a final bit flag :
the highest bit of latest byte is a final-bit-flag.
Consequently, a last byte of `0` is not possible.
And the final-bit-flag itself is not part of the useful bitstream.
Hence, the last byte contains between 0 and 7 useful bits.

Starting from the end,
it's possible to read the bitstream in a little-endian fashion,
keeping track of already used bits.

Reading the last `maxBits` bits,
it's then possible to compare extracted value to decoding table,
determining the symbol to decode and number of bits to discard.

The process continues up to reading the required number of symbols per stream.
If a bitstream is not entirely and exactly consumed,
hence reaching exactly its beginning position with _all_ bits consumed,
the decoding process is considered faulty.


### Sequences section

A compressed block is a succession of _sequences_ .
A sequence is a literal copy command, followed by a match copy command.
A literal copy command specifies a length.
It is the number of bytes to be copied (or extracted) from the literal section.
A match copy command specifies an offset and a length.
The offset gives the position to copy from,
which can be within a previous block.

There are 3 symbol types, `literalLength`, `matchLength` and `offset`,
which are encoded together, interleaved in a single _bitstream_.

Each symbol is a _code_ in its own context,
which specifies a baseline and a number of bits to add.
_Codes_ are FSE compressed,
and interleaved with raw additional bits in the same bitstream.

The Sequences section starts by a header,
followed by optional Probability tables for each symbol type,
followed by the bitstream.

| Header | [LitLengthTable] | [OffsetTable] | [MatchLengthTable] | bitStream |
| ------ | ---------------- | ------------- | ------------------ | --------- |

To decode the Sequence section, it's required to know its size.
This size is deducted from `blockSize - literalSectionSize`.


#### Sequences section header

Consists in 2 items :
- Nb of Sequences
- Flags providing Symbol compression types

__Nb of Sequences__

This is a variable size field, `nbSeqs`, using between 1 and 3 bytes.
Let's call its first byte `byte0`.
- `if (byte0 == 0)` : there are no sequences.
            The sequence section stops there.
            Regenerated content is defined entirely by literals section.
- `if (byte0 < 128)` : `nbSeqs = byte0;` . Uses 1 byte.
- `if (byte0 < 255)` : `nbSeqs = ((byte0-128) << 8) + byte1;` . Uses 2 bytes.
- `if (byte0 == 255)`: `nbSeqs = byte1 + (byte2<<8) + 0x7F00;` . Uses 3 bytes.

__Symbol encoding modes__

This is a single byte, defining the compression mode of each symbol type.

|  BitNb  |   7-6  |   5-4  |   3-2  |    1-0   |
| ------- | ------ | ------ | ------ | -------- |
|FieldName| LLType | OFType | MLType | Reserved |

The last field, `Reserved`, must be all-zeroes.

`LLType`, `OFType` and `MLType` define the compression mode of
Literal Lengths, Offsets and Match Lengths respectively.

They follow the same enumeration :

|       Value      |    0   |  1  |      2     |    3   |
| ---------------- | ------ | --- | ---------- | ------ |
| Compression Mode | predef | RLE | Compressed | Repeat |

- "predef" : uses a pre-defined distribution table.
- "RLE" : it's a single code, repeated `nbSeqs` times.
- "Repeat" : re-use distribution table from previous compressed block.
- "Compressed" : standard FSE compression.
          A distribution table will be present.
          It will be described in [next part](#distribution-tables).

#### Symbols decoding

##### Literal Lengths codes

Literal lengths codes are values ranging from `0` to `35` included.
They define lengths from 0 to 131071 bytes.

|  Code  | 0-15 |
| ------ | ---- |
| length | Code |
| nbBits |   0  |


|   Code   |  16  |  17  |  18  |  19  |  20  |  21  |  22  |  23  |
| -------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Baseline |  16  |  18  |  20  |  22  |  24  |  28  |  32  |  40  |
| nb Bits  |   1  |   1  |   1  |   1  |   2  |   2  |   3  |   3  |

|   Code   |  24  |  25  |  26  |  27  |  28  |  29  |  30  |  31  |
| -------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Baseline |  48  |  64  |  128 |  256 |  512 | 1024 | 2048 | 4096 |
| nb Bits  |   4  |   6  |   7  |   8  |   9  |  10  |  11  |  12  |

|   Code   |  32  |  33  |  34  |  35  |
| -------- | ---- | ---- | ---- | ---- |
| Baseline | 8192 |16384 |32768 |65536 |
| nb Bits  |  13  |  14  |  15  |  16  |

__Default distribution__

When "compression mode" is "predef"",
a pre-defined distribution is used for FSE compression.

Below is its definition. It uses an accuracy of 6 bits (64 states).
```
short literalLengths_defaultDistribution[36] =
        { 4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1,
          2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1,
         -1,-1,-1,-1 };
```

##### Match Lengths codes

Match lengths codes are values ranging from `0` to `52` included.
They define lengths from 3 to 131074 bytes.

|  Code  |   0-31   |
| ------ | -------- |
| value  | Code + 3 |
| nbBits |     0    |

|   Code   |  32  |  33  |  34  |  35  |  36  |  37  |  38  |  39  |
| -------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Baseline |  35  |  37  |  39  |  41  |  43  |  47  |  51  |  59  |
| nb Bits  |   1  |   1  |   1  |   1  |   2  |   2  |   3  |   3  |

|   Code   |  40  |  41  |  42  |  43  |  44  |  45  |  46  |  47  |
| -------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Baseline |  67  |  83  |  99  |  131 |  258 |  514 | 1026 | 2050 |
| nb Bits  |   4  |   4  |   5  |   7  |   8  |   9  |  10  |  11  |

|   Code   |  48  |  49  |  50  |  51  |  52  |
| -------- | ---- | ---- | ---- | ---- | ---- |
| Baseline | 4098 | 8194 |16486 |32770 |65538 |
| nb Bits  |  12  |  13  |  14  |  15  |  16  |

__Default distribution__

When "compression mode" is defined as "predef",
a pre-defined distribution is used for FSE compression.

Here is its definition. It uses an accuracy of 6 bits (64 states).
```
short matchLengths_defaultDistribution[53] =
        { 1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,-1,-1,
         -1,-1,-1,-1,-1 };
```

##### Offset codes

Offset codes are values ranging from `0` to `N`,
with `N` being limited by maximum backreference distance.

A decoder is free to limit its maximum `N` supported.
Recommendation is to support at least up to `22`.
For information, at the time of this writing.
the reference decoder supports a maximum `N` value of `28` in 64-bits mode.

An offset code is also the nb of additional bits to read,
and can be translated into an `OFValue` using the following formulae :

```
OFValue = (1 << offsetCode) + readNBits(offsetCode);
if (OFValue > 3) offset = OFValue - 3;
```

OFValue from 1 to 3 are special : they define "repeat codes",
which means one of the previous offsets will be repeated.
They are sorted in recency order, with 1 meaning the most recent one.
See [Repeat offsets](#repeat-offsets) paragraph.

__Default distribution__

When "compression mode" is defined as "predef",
a pre-defined distribution is used for FSE compression.

Here is its definition. It uses an accuracy of 5 bits (32 states),
and supports a maximum `N` of 28, allowing offset values up to 536,870,908 .

If any sequence in the compressed block requires an offset larger than this,
it's not possible to use the default distribution to represent it.

```
short offsetCodes_defaultDistribution[53] =
        { 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1,-1,-1,-1,-1,-1 };
```

#### Distribution tables

Following the header, up to 3 distribution tables can be described.
When present, they are in this order :
- Literal lengthes
- Offsets
- Match Lengthes

The content to decode depends on their respective encoding mode :
- Predef : no content. Use pre-defined distribution table.
- RLE : 1 byte. This is the only code to use across the whole compressed block.
- FSE : A distribution table is present.
- Repeat mode : no content. Re-use distribution from previous compressed block.

##### FSE distribution table : condensed format

An FSE distribution table describes the probabilities of all symbols
from `0` to the last present one (included)
on a normalized scale of `1 << AccuracyLog` .

It's a bitstream which is read forward, in little-endian fashion.
It's not necessary to know its exact size,
since it will be discovered and reported by the decoding process.

The bitstream starts by reporting on which scale it operates.
`AccuracyLog = low4bits + 5;`
Note that maximum `AccuracyLog` for literal and match lengthes is `9`,
and for offsets it is `8`. Higher values are considered errors.

Then follow each symbol value, from `0` to last present one.
The nb of bits used by each field is variable.
It depends on :

- Remaining probabilities + 1 :
  __example__ :
  Presuming an AccuracyLog of 8,
  and presuming 100 probabilities points have already been distributed,
  the decoder may read any value from `0` to `255 - 100 + 1 == 156` (included).
  Therefore, it must read `log2sup(156) == 8` bits.

- Value decoded : small values use 1 less bit :
  __example__ :
  Presuming values from 0 to 156 (included) are possible,
  255-156 = 99 values are remaining in an 8-bits field.
  They are used this way :
  first 99 values (hence from 0 to 98) use only 7 bits,
  values from 99 to 156 use 8 bits.
  This is achieved through this scheme :

  | Value read | Value decoded | nb Bits used |
  | ---------- | ------------- | ------------ |
  |   0 -  98  |   0 -  98     |  7           |
  |  99 - 127  |  99 - 127     |  8           |
  | 128 - 226  |   0 -  98     |  7           |
  | 227 - 255  | 128 - 156     |  8           |

Symbols probabilities are read one by one, in order.

Probability is obtained from Value decoded by following formulae :
`Proba = value - 1;`

It means value `0` becomes negative probability `-1`.
`-1` is a special probability, which means `less than 1`.
Its effect on distribution table is described in [next paragraph].
For the purpose of calculating cumulated distribution, it counts as one.

[next paragraph]:#fse-decoding--from-normalized-distribution-to-decoding-tables

When a symbol has a probability of `zero`,
it is followed by a 2-bits repeat flag.
This repeat flag tells how many probabilities of zeroes follow the current one.
It provides a number ranging from 0 to 3.
If it is a 3, another 2-bits repeat flag follows, and so on.

When last symbol reaches cumulated total of `1 << AccuracyLog`,
decoding is complete.
If the last symbol makes cumulated total go above `1 << AccuracyLog`,
distribution is considered corrupted.

Then the decoder can tell how many bytes were used in this process,
and how many symbols are present.
The bitstream consumes a round number of bytes.
Any remaining bit within the last byte is just unused.

##### FSE decoding : from normalized distribution to decoding tables

The distribution of normalized probabilities is enough
to create a unique decoding table.

It follows the following build rule :

The table has a size of `tableSize = 1 << AccuracyLog;`.
Each cell describes the symbol decoded,
and instructions to get the next state.

Symbols are scanned in their natural order for `less than 1` probabilities.
Symbols with this probability are being attributed a single cell,
starting from the end of the table.
These symbols define a full state reset, reading `AccuracyLog` bits.

All remaining symbols are sorted in their natural order.
Starting from symbol `0` and table position `0`,
each symbol gets attributed as many cells as its probability.
Cell allocation is spreaded, not linear :
each successor position follow this rule :

```
position += (tableSize>>1) + (tableSize>>3) + 3;
position &= tableSize-1;
```

A position is skipped if already occupied,
typically by a "less than 1" probability symbol.

The result is a list of state values.
Each state will decode the current symbol.

To get the Number of bits and baseline required for next state,
it's first necessary to sort all states in their natural order.
The lower states will need 1 more bit than higher ones.

__Example__ :
Presuming a symbol has a probability of 5.
It receives 5 state values. States are sorted in natural order.

Next power of 2 is 8.
Space of probabilities is divided into 8 equal parts.
Presuming the AccuracyLog is 7, it defines 128 states.
Divided by 8, each share is 16 large.

In order to reach 8, 8-5=3 lowest states will count "double",
taking shares twice larger,
requiring one more bit in the process.

Numbering starts from higher states using less bits.

| state order |   0   |   1   |    2   |   3  |   4   |
| ----------- | ----- | ----- | ------ | ---- | ----- |
| width       |  32   |  32   |   32   |  16  |  16   |
| nb Bits     |   5   |   5   |    5   |   4  |   4   |
| range nb    |   2   |   4   |    6   |   0  |   1   |
| baseline    |  32   |  64   |   96   |   0  |  16   |
| range       | 32-63 | 64-95 | 96-127 | 0-15 | 16-31 |

Next state is determined from current state
by reading the required number of bits, and adding the specified baseline.


#### Bitstream

All sequences are stored in a single bitstream, read _backward_.
It is therefore necessary to know the bitstream size,
which is deducted from compressed block size.

The last useful bit of the stream is followed by an end-bit-flag.
Highest bit of last byte is this flag.
It does not belong to the useful part of the bitstream.
Therefore, last byte has 0-7 useful bits.
Note that it also means that last byte cannot be `0`.

##### Starting states

The bitstream starts with initial state values,
each using the required number of bits in their respective _accuracy_,
decoded previously from their normalized distribution.

It starts by `Literal Length State`,
followed by `Offset State`,
and finally `Match Length State`.

Reminder : always keep in mind that all values are read _backward_.

##### Decoding a sequence

A state gives a code.
A code provides a baseline and number of bits to add.
See [Symbol Decoding] section for details on each symbol.

Decoding starts by reading the nb of bits required to decode offset.
It then does the same for match length,
and then for literal length.

Offset / matchLength / litLength define a sequence.
It starts by inserting the number of literals defined by `litLength`,
then continue by copying `matchLength` bytes from `currentPos - offset`.

The next operation is to update states.
Using rules pre-calculated in the decoding tables,
`Literal Length State` is updated,
followed by `Match Length State`,
and then `Offset State`.

This operation will be repeated `NbSeqs` times.
At the end, the bitstream shall be entirely consumed,
otherwise bitstream is considered corrupted.

[Symbol Decoding]:#symbols-decoding

##### Repeat offsets

As seen in [Offset Codes], the first 3 values define a repeated offset.
They are sorted in recency order, with 1 meaning "most recent one".

There is an exception though, when current sequence's literal length is `0`.
In which case, 1 would just make previous match longer.
Therefore, in such case, 1 means in fact 2, and 2 is impossible.
Meaning of 3 is unmodified.

Repeat offsets start with the following values : 1, 4 and 8 (in order).

Then each block receives its start value from previous compressed block.
Note that non-compressed blocks are skipped,
they do not contribute to offset history.

[Offset Codes]: #offset-codes

###### Offset updates rules

When the new offset is a normal one,
offset history is simply translated by one position,
with the new offset taking first spot.

- When repeat offset 1 (most recent) is used, history is unmodified.
- When repeat offset 2 is used, it's swapped with offset 1.
- When repeat offset 3 is used, it takes first spot,
  pushing the other ones by one position.


Dictionary format
-----------------

`zstd` is compatible with "pure content" dictionaries, free of any format restriction.
But dictionaries created by `zstd --train` follow a format, described here.

__Pre-requisites__ : a dictionary has a known length,
                     defined either by a buffer limit, or a file size.

| Header | DictID | Stats | Content |
| ------ | ------ | ----- | ------- |

__Header__ : 4 bytes ID, value 0xEC30A437, Little Endian format

__Dict_ID__ : 4 bytes, stored in Little Endian format.
              DictID can be any value, except 0 (which means no DictID).
              It's used by decoders to check if they use the correct dictionary.
              _Reserved ranges :_
              If the frame is going to be distributed in a private environment,
              any dictionary ID can be used.
              However, for public distribution of compressed frames,
              some ranges are reserved for future use :

              - low range : 1 - 32767 : reserved
              - high range : >= (2^31) : reserved

__Stats__ : Entropy tables, following the same format as a [compressed blocks].
            They are stored in following order :
            Huffman tables for literals, FSE table for offset,
            FSE table for matchLenth, and FSE table for litLength.
            It's finally followed by 3 offset values, populating recent offsets,
            stored in order, 4-bytes little endian each, for a total of 12 bytes.

__Content__ : Where the actual dictionary content is.
              Content size depends on Dictionary size.

[compressed blocks]: #the-format-of-compressed_block


Version changes
---------------
- 0.2.0 : numerous format adjustments for zstd v0.8
- 0.1.2 : limit huffman tree depth to 11 bits
- 0.1.1 : reserved dictID ranges
- 0.1.0 : initial release
