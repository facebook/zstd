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

0.2.3 (27/01/17)


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
In this document:
- square brackets i.e. `[` and `]` are used to indicate optional fields or parameters.
- a naming convention for identifiers is `Mixed_Case_With_Underscores`

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

4 Bytes, little-endian format.
Value : 0x184D2A5?, which means any value from 0x184D2A50 to 0x184D2A5F.
All 16 values are valid to identify a skippable frame.

__`Frame_Size`__

This is the size, in bytes, of the following `User_Data`
(without including the magic number nor the size field itself).
This field is represented using 4 Bytes, little-endian format, unsigned 32-bits.
This means `User_Data` can’t be bigger than (2^32-1) bytes.

__`User_Data`__

The `User_Data` can be anything. Data will just be skipped by the decoder.



General Structure of Zstandard Frame format
-------------------------------------------
The structure of a single Zstandard frame is following:

| `Magic_Number` | `Frame_Header` |`Data_Block`| [More data blocks] | [`Content_Checksum`] |
|:--------------:|:--------------:|:----------:| ------------------ |:--------------------:|
| 4 bytes        |  2-14 bytes    | n bytes    |                    |   0-4 bytes          |

__`Magic_Number`__

4 Bytes, little-endian format.
Value : 0xFD2FB528

__`Frame_Header`__

2 to 14 Bytes, detailed in [next part](#the-structure-of-frame_header).

__`Data_Block`__

Detailed in [next chapter](#the-structure-of-data_block).
That’s where compressed data is stored.

__`Content_Checksum`__

An optional 32-bit checksum, only present if `Content_Checksum_flag` is set.
The content checksum is the result
of [xxh64() hash function](http://www.xxhash.org)
digesting the original (decoded) data as input, and a seed of zero.
The low 4 bytes of the checksum are stored in little endian format.


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

__`Frame_Content_Size_flag`__

This is a 2-bits flag (`= Frame_Header_Descriptor >> 6`),
specifying if decompressed data size is provided within the header.
The `Flag_Value` can be converted into `Field_Size`,
which is the number of bytes used by `Frame_Content_Size`
according to the following table:

|`Flag_Value`|    0   |  1  |  2  |  3  |
| ---------- | ------ | --- | --- | --- |
|`Field_Size`| 0 or 1 |  2  |  4  |  8  |

When `Flag_Value` is `0`, `Field_Size` depends on `Single_Segment_flag` :
if `Single_Segment_flag` is set, `Field_Size` is 1.
Otherwise, `Field_Size` is 0 (content size not provided).

__`Single_Segment_flag`__

If this flag is set,
data must be regenerated within a single continuous memory segment.

In this case, `Frame_Content_Size` is necessarily present,
but `Window_Descriptor` byte is skipped.
As a consequence, the decoder must allocate a memory segment
of size equal or bigger than `Frame_Content_Size`.

In order to preserve the decoder from unreasonable memory requirement,
a decoder can reject a compressed frame
which requests a memory size beyond decoder's authorized range.

For broader compatibility, decoders are recommended to support
memory sizes of at least 8 MB.
This is just a recommendation,
each decoder is free to support higher or lower limits,
depending on local limitations.

__`Unused_bit`__

The value of this bit should be set to zero.
A decoder compliant with this specification version shall not interpret it.
It might be used in a future version,
to signal a property which is not mandatory to properly decode the frame.

__`Reserved_bit`__

This bit is reserved for some future feature.
Its value _must be zero_.
A decoder compliant with this specification version must ensure it is not set.
This bit may be used in a future revision,
to signal a feature that must be interpreted to decode the frame correctly.

__`Content_Checksum_flag`__

If this flag is set, a 32-bits `Content_Checksum` will be present at frame's end.
See `Content_Checksum` paragraph.

__`Dictionary_ID_flag`__

This is a 2-bits flag (`= FHD & 3`),
telling if a dictionary ID is provided within the header.
It also specifies the size of this field as `Field_Size`.

|`Flag_Value`|  0  |  1  |  2  |  3  |
| ---------- | --- | --- | --- | --- |
|`Field_Size`|  0  |  1  |  2  |  4  |

### `Window_Descriptor`

Provides guarantees on maximum back-reference distance
that will be used within compressed data.
This information is important for decoders to allocate enough memory.

The `Window_Descriptor` byte is optional. It is absent when `Single_Segment_flag` is set.
In this case, the maximum back-reference distance is the content size itself,
which can be any value from 1 to 2^64-1 bytes (16 EB).

| Bit numbers |     7-3    |     2-0    |
| ----------- | ---------- | ---------- |
| Field name  | `Exponent` | `Mantissa` |

Maximum distance is given by the following formulas :
```
windowLog = 10 + Exponent;
windowBase = 1 << windowLog;
windowAdd = (windowBase / 8) * Mantissa;
Window_Size = windowBase + windowAdd;
```
The minimum window size is 1 KB.
The maximum size is `15*(1<<38)` bytes, which is 1.875 TB.

To properly decode compressed data,
a decoder will need to allocate a buffer of at least `Window_Size` bytes.

In order to preserve decoder from unreasonable memory requirements,
a decoder can refuse a compressed frame
which requests a memory size beyond decoder's authorized range.

For improved interoperability,
decoders are recommended to be compatible with window sizes of 8 MB,
and encoders are recommended to not request more than 8 MB.
It's merely a recommendation though,
decoders are free to support larger or lower limits,
depending on local limitations.

### `Dictionary_ID`

This is a variable size field, which contains
the ID of the dictionary required to properly decode the frame.
Note that this field is optional. When it's not present,
it's up to the caller to make sure it uses the correct dictionary.
Format is little-endian.

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

This is the original (uncompressed) size. This information is optional.
The `Field_Size` is provided according to value of `Frame_Content_Size_flag`.
The `Field_Size` can be equal to 0 (not present), 1, 2, 4 or 8 bytes.
Format is little-endian.

| `Field_Size` |    Range   |
| ------------ | ---------- |
|      1       |   0 - 255  |
|      2       | 256 - 65791|
|      4       | 0 - 2^32-1 |
|      8       | 0 - 2^64-1 |

When `Field_Size` is 1, 4 or 8 bytes, the value is read directly.
When `Field_Size` is 2, _the offset of 256 is added_.
It's allowed to represent a small size (for example `18`) using any compatible variant.


The structure of `Data_Block`
-----------------------------
The structure of `Data_Block` is following:

| `Last_Block` | `Block_Type` | `Block_Size` | `Block_Content` |
|:------------:|:------------:|:------------:|:---------------:|
|   1 bit      |  2 bits      |  21 bits     |  n bytes        |

The block header (`Last_Block`, `Block_Type`, and `Block_Size`) uses 3-bytes.

__`Last_Block`__

The lowest bit signals if this block is the last one.
Frame ends right after this block.
It may be followed by an optional `Content_Checksum` .

__`Block_Type` and `Block_Size`__

The next 2 bits represent the `Block_Type`,
while the remaining 21 bits represent the `Block_Size`.
Format is __little-endian__.

There are 4 block types :

|    Value     |      0      |     1       |  2                 |    3      |
| ------------ | ----------- | ----------- | ------------------ | --------- |
| `Block_Type` | `Raw_Block` | `RLE_Block` | `Compressed_Block` | `Reserved`|

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
- `Reserved` - this is not a block.
  This value cannot be used with current version of this specification.

Block sizes must respect a few rules :
- In compressed mode, compressed size is always strictly less than decompressed size.
- Block decompressed size is always <= maximum back-reference distance.
- Block decompressed size is always <= 128 KB.


__`Block_Content`__

The `Block_Content` is where the actual data to decode stands.
It might be compressed or not, depending on previous field indications.
A data block is not necessarily "full" :
since an arbitrary “flush” may happen anytime,
block decompressed content can be any size,
up to `Block_Maximum_Decompressed_Size`, which is the smallest of :
- Maximum back-reference distance
- 128 KB



The format of `Compressed_Block`
--------------------------------
The size of `Compressed_Block` must be provided using `Block_Size` field from `Data_Block`.
The `Compressed_Block` has a guaranteed maximum regenerated size,
in order to properly allocate destination buffer.
See [`Data_Block`](#the-structure-of-data_block) for more details.

A compressed block consists of 2 sections :
- [`Literals_Section`](#literals_section)
- [`Sequences_Section`](#sequences_section)

### Prerequisites
To decode a compressed block, the following elements are necessary :
- Previous decoded blocks, up to a distance of `Window_Size`,
  or all previous blocks when `Single_Segment_flag` is set.
- List of "recent offsets" from previous compressed block.
- Decoding tables of previous compressed block for each symbol type
  (literals, literals lengths, match lengths, offsets).


### `Literals_Section`

During sequence phase, literals will be entangled with match copy operations.
All literals are regrouped in the first part of the block.
They can be decoded first, and then copied during sequence operations,
or they can be decoded on the flow, as needed by sequence commands.

| `Literals_Section_Header` | [`Huffman_Tree_Description`] | Stream1 | [Stream2] | [Stream3] | [Stream4] |
| ------------------------- | ---------------------------- | ------- | --------- | --------- | --------- |

Literals can be stored uncompressed or compressed using Huffman prefix codes.
When compressed, an optional tree description can be present,
followed by 1 or 4 streams.


#### `Literals_Section_Header`

Header is in charge of describing how literals are packed.
It's a byte-aligned variable-size bitfield, ranging from 1 to 5 bytes,
using little-endian convention.

| `Literals_Block_Type` | `Size_Format` | `Regenerated_Size` | [`Compressed_Size`] |
| --------------------- | ------------- | ------------------ | ----------------- |
|   2 bits              |  1 - 2 bits   |    5 - 20 bits     |    0 - 18 bits    |

In this representation, bits on the left are smallest bits.

__`Literals_Block_Type`__

This field uses 2 lowest bits of first byte, describing 4 different block types :

| `Literals_Block_Type`         | Value |
| ----------------------------- | ----- |
| `Raw_Literals_Block`          |   0   |
| `RLE_Literals_Block`          |   1   |
| `Compressed_Literals_Block`   |   2   |
| `Repeat_Stats_Literals_Block` |   3   |

- `Raw_Literals_Block` - Literals are stored uncompressed.
- `RLE_Literals_Block` - Literals consist of a single byte value repeated N times.
- `Compressed_Literals_Block` - This is a standard Huffman-compressed block,
        starting with a Huffman tree description.
        See details below.
- `Repeat_Stats_Literals_Block` - This is a Huffman-compressed block,
        using Huffman tree _from previous Huffman-compressed literals block_.
        Huffman tree description will be skipped.

__`Size_Format`__

`Size_Format` is divided into 2 families :

- For `Compressed_Block`, it requires to decode both `Compressed_Size`
  and `Regenerated_Size` (the decompressed size). It will also decode the number of streams.
- For `Raw_Literals_Block` and `RLE_Literals_Block` it's enough to decode `Regenerated_Size`.

For values spanning several bytes, convention is little-endian.

__`Size_Format` for `Raw_Literals_Block` and `RLE_Literals_Block`__ :

- Value ?0 : `Size_Format` uses 1 bit.
               `Regenerated_Size` uses 5 bits (0-31).
               `Literals_Section_Header` has 1 byte.
               `Regenerated_Size = Header[0]>>3`
- Value 01 : `Size_Format` uses 2 bits.
               `Regenerated_Size` uses 12 bits (0-4095).
               `Literals_Section_Header` has 2 bytes.
               `Regenerated_Size = (Header[0]>>4) + (Header[1]<<4)`
- Value 11 : `Size_Format` uses 2 bits.
               `Regenerated_Size` uses 20 bits (0-1048575).
               `Literals_Section_Header` has 3 bytes.
               `Regenerated_Size = (Header[0]>>4) + (Header[1]<<4) + (Header[2]<<12)`

Note : it's allowed to represent a short value (for example `13`)
using a long format, accepting the increased compressed data size.

__`Size_Format` for `Compressed_Literals_Block` and `Repeat_Stats_Literals_Block`__ :

- Value 00 : _A single stream_.
               Both `Compressed_Size` and `Regenerated_Size` use 10 bits (0-1023).
               `Literals_Section_Header` has 3 bytes.
- Value 01 : 4 streams.
               Both `Compressed_Size` and `Regenerated_Size` use 10 bits (0-1023).
               `Literals_Section_Header` has 3 bytes.
- Value 10 : 4 streams.
               Both `Compressed_Size` and `Regenerated_Size` use 14 bits (0-16383).
               `Literals_Section_Header` has 4 bytes.
- Value 11 : 4 streams.
               Both `Compressed_Size` and `Regenerated_Size` use 18 bits (0-262143).
               `Literals_Section_Header` has 5 bytes.

Both `Compressed_Size` and `Regenerated_Size` fields follow little-endian convention.
Note: `Compressed_Size` __includes__ the size of the Huffman Tree description if it
is present.

#### `Huffman_Tree_Description`

This section is only present when `Literals_Block_Type` type is `Compressed_Literals_Block` (`2`).

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
are represented by `Weight` with values from `0` to `Max_Number_of_Bits`.
Transformation from `Weight` to `Number_of_Bits` follows this formula :
```
Number_of_Bits = Weight ? (Max_Number_of_Bits + 1 - Weight) : 0
```
The last symbol's `Weight` is deduced from previously decoded ones,
by completing to the nearest power of 2.
This power of 2 gives `Max_Number_of_Bits`, the depth of the current tree.

__Example__ :
Let's presume the following Huffman tree must be described :

|     literal      |  0  |  1  |  2  |  3  |  4  |  5  |
| ---------------- | --- | --- | --- | --- | --- | --- |
| `Number_of_Bits` |  1  |  2  |  3  |  0  |  4  |  4  |

The tree depth is 4, since its smallest element uses 4 bits.
Value `5` will not be listed as it can be determined from the values for 0-4,
nor will values above `5` as they are all 0.
Values from `0` to `4` will be listed using `Weight` instead of `Number_of_Bits`.
Weight formula is :
```
Weight = Number_of_Bits ? (Max_Number_of_Bits + 1 - Number_of_Bits) : 0
```
It gives the following series of weights :

| literal  |  0  |  1  |  2  |  3  |  4  |
| -------- | --- | --- | --- | --- | --- |
| `Weight` |  4  |  3  |  2  |  0  |  1  |

The decoder will do the inverse operation :
having collected weights of literals from `0` to `4`,
it knows the last literal, `5`, is present with a non-zero weight.
The weight of `5` can be determined by advancing to the next power of 2.
The sum of `2^(Weight-1)` (excluding 0's) is :
`8 + 4 + 2 + 0 + 1 = 15`.
Nearest power of 2 is 16.
Therefore, `Max_Number_of_Bits = 4` and `Weight[5] = 1`.

##### Huffman Tree header

This is a single byte value (0-255),
which describes how to decode the list of weights.

- if `headerByte` >= 128 : this is a direct representation,
  where each `Weight` is written directly as a 4 bits field (0-15).
  They are encoded forward, 2 weights to a byte with the first weight taking
  the top four bits and the second taking the bottom four (e.g. the following
  operations could be used to read the weights:
  `Weight[0] = (Byte[0] >> 4), Weight[1] = (Byte[0] & 0xf)`, etc.).
  The full representation occupies `((Number_of_Symbols+1)/2)` bytes,
  meaning it uses a last full byte even if `Number_of_Symbols` is odd.
  `Number_of_Symbols = headerByte - 127`.
  Note that maximum `Number_of_Symbols` is 255-127 = 128.
  A larger series must necessarily use FSE compression.

- if `headerByte` < 128 :
  the series of weights is compressed by FSE.
  The length of the FSE-compressed series is equal to `headerByte` (0-127).

##### Finite State Entropy (FSE) compression of Huffman weights

FSE decoding uses three operations: `Init_State`, `Decode_Symbol`, and `Update_State`.
`Init_State` reads in the initial state value from a bitstream,
`Decode_Symbol` outputs a symbol based on the current state,
and `Update_State` goes to a new state based on the current state and some number of consumed bits.

FSE streams must be read in reverse from the order they're encoded in,
so bitstreams start at a certain offset and works backwards towards their base.

For more on how FSE bitstreams work, see [Finite State Entropy].

[Finite State Entropy]:https://github.com/Cyan4973/FiniteStateEntropy/

The series of Huffman weights is compressed using FSE compression.
It's a single bitstream with 2 interleaved states,
sharing a single distribution table.

To decode an FSE bitstream, it is necessary to know its compressed size.
Compressed size is provided by `headerByte`.
It's also necessary to know its _maximum possible_ decompressed size,
which is `255`, since literal values span from `0` to `255`,
and last symbol's weight is not represented.

An FSE bitstream starts by a header, describing probabilities distribution.
It will create a Decoding Table.
The table must be pre-allocated, so a maximum accuracy must be fixed.
For a list of Huffman weights, maximum accuracy is 7 bits.

The FSE header format is [described in a relevant chapter](#fse-distribution-table--condensed-format),
as well as the [FSE bitstream](#bitstream).
The main difference is that Huffman header compression uses 2 states,
which share the same FSE distribution table.
The first state (`State1`) encodes the even indexed symbols,
and the second (`State2`) encodes the odd indexes.
State1 is initialized first, and then State2, and they take turns decoding
a single symbol and updating their state.

The number of symbols to decode is determined
by tracking bitStream overflow condition:
If updating state after decoding a symbol would require more bits than
remain in the stream, it is assumed the extra bits are 0.  Then,
the symbols for each of the final states are decoded and the process is complete.

##### Conversion from weights to Huffman prefix codes

All present symbols shall now have a `Weight` value.
It is possible to transform weights into Number_of_Bits, using this formula:
```
Number_of_Bits = Number_of_Bits ? Max_Number_of_Bits + 1 - Weight : 0
```
Symbols are sorted by `Weight`. Within same `Weight`, symbols keep natural order.
Symbols with a `Weight` of zero are removed.
Then, starting from lowest weight, prefix codes are distributed in order.

__Example__ :
Let's presume the following list of weights has been decoded :

| Literal  |  0  |  1  |  2  |  3  |  4  |  5  |
| -------- | --- | --- | --- | --- | --- | --- |
| `Weight` |  4  |  3  |  2  |  0  |  1  |  1  |

Sorted by weight and then natural order,
it gives the following distribution :

| Literal          |  3  |  4  |  5  |  2  |  1  |   0  |
| ---------------- | --- | --- | --- | --- | --- | ---- |
| `Weight`         |  0  |  1  |  1  |  2  |  3  |   4  |
| `Number_of_Bits` |  0  |  4  |  4  |  3  |  2  |   1  |
| prefix codes     | N/A | 0000| 0001| 001 | 01  |   1  |


#### The content of Huffman-compressed literal stream

##### Bitstreams sizes

As seen in a previous paragraph,
there are 2 types of Huffman-compressed literals :
a single stream and 4 streams.

Encoding using 4 streams is useful for CPU with multiple execution units and out-of-order operations.
Since each stream can be decoded independently,
it's possible to decode them up to 4x faster than a single stream,
presuming the CPU has enough parallelism available.

For single stream, header provides both the compressed and regenerated size.
For 4 streams though,
header only provides compressed and regenerated size of all 4 streams combined.
In order to properly decode the 4 streams,
it's necessary to know the compressed and regenerated size of each stream.

Regenerated size of each stream can be calculated by `(totalSize+3)/4`,
except for last one, which can be up to 3 bytes smaller, to reach `totalSize`.

Compressed size is provided explicitly : in the 4-streams variant,
bitstreams are preceded by 3 unsigned little-endian 16-bits values.
Each value represents the compressed size of one stream, in order.
The last stream size is deducted from total compressed size
and from previously decoded stream sizes :

`stream4CSize = totalCSize - 6 - stream1CSize - stream2CSize - stream3CSize`.


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

For example, if the literal sequence "0145" was encoded using the prefix codes above,
it would be encoded as:
```
00000001 01110000
```

|Symbol  |   5  |   4  |  1 | 0 | Padding |
|--------|------|------|----|---|---------|
|Encoding|`0000`|`0001`|`01`|`1`| `10000` |

Starting from the end,
it's possible to read the bitstream in a little-endian fashion,
keeping track of already used bits.  Since the bitstream is encoded in reverse
order, by starting at the end the symbols can be read in forward order.

Reading the last `Max_Number_of_Bits` bits,
it's then possible to compare extracted value to decoding table,
determining the symbol to decode and number of bits to discard.

The process continues up to reading the required number of symbols per stream.
If a bitstream is not entirely and exactly consumed,
hence reaching exactly its beginning position with _all_ bits consumed,
the decoding process is considered faulty.

### `Sequences_Section`

A compressed block is a succession of _sequences_ .
A sequence is a literal copy command, followed by a match copy command.
A literal copy command specifies a length.
It is the number of bytes to be copied (or extracted) from the literal section.
A match copy command specifies an offset and a length.
The offset gives the position to copy from,
which can be within a previous block.

When all _sequences_ are decoded,
if there is are any literals left in the _literal section_,
these bytes are added at the end of the block.

The `Sequences_Section` regroup all symbols required to decode commands.
There are 3 symbol types : literals lengths, offsets and match lengths.
They are encoded together, interleaved, in a single _bitstream_.

The `Sequences_Section` starts by a header,
followed by optional probability tables for each symbol type,
followed by the bitstream.

| `Sequences_Section_Header` | [`Literals_Length_Table`] | [`Offset_Table`] | [`Match_Length_Table`] | bitStream |
| -------------------------- | ------------------------- | ---------------- | ---------------------- | --------- |

To decode the `Sequences_Section`, it's required to know its size.
This size is deducted from `blockSize - literalSectionSize`.


#### `Sequences_Section_Header`

Consists of 2 items:
- `Number_of_Sequences`
- Symbol compression modes

__`Number_of_Sequences`__

This is a variable size field using between 1 and 3 bytes.
Let's call its first byte `byte0`.
- `if (byte0 == 0)` : there are no sequences.
            The sequence section stops there.
            Regenerated content is defined entirely by literals section.
- `if (byte0 < 128)` : `Number_of_Sequences = byte0` . Uses 1 byte.
- `if (byte0 < 255)` : `Number_of_Sequences = ((byte0-128) << 8) + byte1` . Uses 2 bytes.
- `if (byte0 == 255)`: `Number_of_Sequences = byte1 + (byte2<<8) + 0x7F00` . Uses 3 bytes.

__Symbol compression modes__

This is a single byte, defining the compression mode of each symbol type.

|Bit number|   7-6                   |   5-4          |   3-2                |     1-0    |
| -------- | ----------------------- | -------------- | -------------------- | ---------- |
|Field name| `Literals_Lengths_Mode` | `Offsets_Mode` | `Match_Lengths_Mode` | `Reserved` |

The last field, `Reserved`, must be all-zeroes.

`Literals_Lengths_Mode`, `Offsets_Mode` and `Match_Lengths_Mode` define the `Compression_Mode` of
literals lengths, offsets, and match lengths respectively.

They follow the same enumeration :

|        Value       |         0         |      1     |           2           |       3       |
| ------------------ | ----------------- | ---------- | --------------------- | ------------- |
| `Compression_Mode` | `Predefined_Mode` | `RLE_Mode` | `FSE_Compressed_Mode` | `Repeat_Mode` |

- `Predefined_Mode` : uses a predefined distribution table.
- `RLE_Mode` : it's a single code, repeated `Number_of_Sequences` times.
- `Repeat_Mode` : re-use distribution table from previous compressed block.
- `FSE_Compressed_Mode` : standard FSE compression.
          A distribution table will be present.
          It will be described in [next part](#distribution-tables).

#### The codes for literals lengths, match lengths, and offsets.

Each symbol is a _code_ in its own context,
which specifies `Baseline` and `Number_of_Bits` to add.
_Codes_ are FSE compressed,
and interleaved with raw additional bits in the same bitstream.

##### Literals length codes

Literals length codes are values ranging from `0` to `35` included.
They define lengths from 0 to 131071 bytes.

| `Literals_Length_Code` |         0-15           |
| ---------------------- | ---------------------- |
| length                 | `Literals_Length_Code` |
| `Number_of_Bits`       |          0             |

| `Literals_Length_Code` |  16  |  17  |  18  |  19  |  20  |  21  |  22  |  23  |
| ---------------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| `Baseline`             |  16  |  18  |  20  |  22  |  24  |  28  |  32  |  40  |
| `Number_of_Bits`       |   1  |   1  |   1  |   1  |   2  |   2  |   3  |   3  |

| `Literals_Length_Code` |  24  |  25  |  26  |  27  |  28  |  29  |  30  |  31  |
| ---------------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| `Baseline`             |  48  |  64  |  128 |  256 |  512 | 1024 | 2048 | 4096 |
| `Number_of_Bits`       |   4  |   6  |   7  |   8  |   9  |  10  |  11  |  12  |

| `Literals_Length_Code` |  32  |  33  |  34  |  35  |
| ---------------------- | ---- | ---- | ---- | ---- |
| `Baseline`             | 8192 |16384 |32768 |65536 |
| `Number_of_Bits`       |  13  |  14  |  15  |  16  |

##### Default distribution for literals length codes

When `Compression_Mode` is `Predefined_Mode`,
a predefined distribution is used for FSE compression.

Its definition is below. It uses an accuracy of 6 bits (64 states).
```
short literalsLength_defaultDistribution[36] =
        { 4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1,
          2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1,
         -1,-1,-1,-1 };
```

##### Match length codes

Match length codes are values ranging from `0` to `52` included.
They define lengths from 3 to 131074 bytes.

| `Match_Length_Code` |         0-31            |
| ------------------- | ----------------------- |
| value               | `Match_Length_Code` + 3 |
| `Number_of_Bits`    |          0              |

| `Match_Length_Code` |  32  |  33  |  34  |  35  |  36  |  37  |  38  |  39  |
| ------------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| `Baseline`          |  35  |  37  |  39  |  41  |  43  |  47  |  51  |  59  |
| `Number_of_Bits`    |   1  |   1  |   1  |   1  |   2  |   2  |   3  |   3  |

| `Match_Length_Code` |  40  |  41  |  42  |  43  |  44  |  45  |  46  |  47  |
| ------------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| `Baseline`          |  67  |  83  |  99  |  131 |  259 |  515 | 1027 | 2051 |
| `Number_of_Bits`    |   4  |   4  |   5  |   7  |   8  |   9  |  10  |  11  |

| `Match_Length_Code` |  48  |  49  |  50  |  51  |  52  |
| ------------------- | ---- | ---- | ---- | ---- | ---- |
| `Baseline`          | 4099 | 8195 |16387 |32771 |65539 |
| `Number_of_Bits`    |  12  |  13  |  14  |  15  |  16  |

##### Default distribution for match length codes

When `Compression_Mode` is defined as `Predefined_Mode`,
a predefined distribution is used for FSE compression.

Its definition is below. It uses an accuracy of 6 bits (64 states).
```
short matchLengths_defaultDistribution[53] =
        { 1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,-1,-1,
         -1,-1,-1,-1,-1 };
```

##### Offset codes

Offset codes are values ranging from `0` to `N`.

A decoder is free to limit its maximum `N` supported.
Recommendation is to support at least up to `22`.
For information, at the time of this writing.
the reference decoder supports a maximum `N` value of `28` in 64-bits mode.

An offset code is also the number of additional bits to read,
and can be translated into an `Offset_Value` using the following formulas :

```
Offset_Value = (1 << offsetCode) + readNBits(offsetCode);
if (Offset_Value > 3) offset = Offset_Value - 3;
```
It means that maximum `Offset_Value` is `(2^(N+1))-1` and it supports back-reference distance up to `(2^(N+1))-4`
but is limited by [maximum back-reference distance](#window_descriptor).

`Offset_Value` from 1 to 3 are special : they define "repeat codes",
which means one of the previous offsets will be repeated.
They are sorted in recency order, with 1 meaning the most recent one.
See [Repeat offsets](#repeat-offsets) paragraph.


##### Default distribution for offset codes

When `Compression_Mode` is defined as `Predefined_Mode`,
a predefined distribution is used for FSE compression.

Below is its definition. It uses an accuracy of 5 bits (32 states),
and supports a maximum `N` of 28, allowing offset values up to 536,870,908 .

If any sequence in the compressed block requires an offset larger than this,
it's not possible to use the default distribution to represent it.

```
short offsetCodes_defaultDistribution[29] =
        { 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1,-1,-1,-1,-1,-1 };
```

#### Distribution tables

Following the header, up to 3 distribution tables can be described.
When present, they are in this order :
- Literals lengths
- Offsets
- Match Lengths

The content to decode depends on their respective encoding mode :
- `Predefined_Mode` : no content. Use the predefined distribution table.
- `RLE_Mode` : 1 byte. This is the only code to use across the whole compressed block.
- `FSE_Compressed_Mode` : A distribution table is present.
- `Repeat_Mode` : no content. Re-use distribution from previous compressed block.

##### FSE distribution table : condensed format

An FSE distribution table describes the probabilities of all symbols
from `0` to the last present one (included)
on a normalized scale of `1 << Accuracy_Log` .

It's a bitstream which is read forward, in little-endian fashion.
It's not necessary to know its exact size,
since it will be discovered and reported by the decoding process.

The bitstream starts by reporting on which scale it operates.
`Accuracy_Log = low4bits + 5`.
Note that maximum `Accuracy_Log` for literal and match lengths is `9`,
and for offsets is `8`. Higher values are considered errors.

Then follows each symbol value, from `0` to last present one.
The number of bits used by each field is variable.
It depends on :

- Remaining probabilities + 1 :
  __example__ :
  Presuming an `Accuracy_Log` of 8,
  and presuming 100 probabilities points have already been distributed,
  the decoder may read any value from `0` to `255 - 100 + 1 == 156` (inclusive).
  Therefore, it must read `log2sup(156) == 8` bits.

- Value decoded : small values use 1 less bit :
  __example__ :
  Presuming values from 0 to 156 (inclusive) are possible,
  255-156 = 99 values are remaining in an 8-bits field.
  They are used this way :
  first 99 values (hence from 0 to 98) use only 7 bits,
  values from 99 to 156 use 8 bits.
  This is achieved through this scheme :

  | Value read | Value decoded | Number of bits used |
  | ---------- | ------------- | ------------------- |
  |   0 -  98  |   0 -  98     |  7                  |
  |  99 - 127  |  99 - 127     |  8                  |
  | 128 - 226  |   0 -  98     |  7                  |
  | 227 - 255  | 128 - 156     |  8                  |

Symbols probabilities are read one by one, in order.

Probability is obtained from Value decoded by following formula :
`Proba = value - 1`

It means value `0` becomes negative probability `-1`.
`-1` is a special probability, which means "less than 1".
Its effect on distribution table is described in [next paragraph].
For the purpose of calculating cumulated distribution, it counts as one.

[next paragraph]:#fse-decoding--from-normalized-distribution-to-decoding-tables

When a symbol has a __probability__ of `zero`,
it is followed by a 2-bits repeat flag.
This repeat flag tells how many probabilities of zeroes follow the current one.
It provides a number ranging from 0 to 3.
If it is a 3, another 2-bits repeat flag follows, and so on.

When last symbol reaches cumulated total of `1 << Accuracy_Log`,
decoding is complete.
If the last symbol makes cumulated total go above `1 << Accuracy_Log`,
distribution is considered corrupted.

Then the decoder can tell how many bytes were used in this process,
and how many symbols are present.
The bitstream consumes a round number of bytes.
Any remaining bit within the last byte is just unused.

##### FSE decoding : from normalized distribution to decoding tables

The distribution of normalized probabilities is enough
to create a unique decoding table.

It follows the following build rule :

The table has a size of `tableSize = 1 << Accuracy_Log`.
Each cell describes the symbol decoded,
and instructions to get the next state.

Symbols are scanned in their natural order for "less than 1" probabilities.
Symbols with this probability are being attributed a single cell,
starting from the end of the table.
These symbols define a full state reset, reading `Accuracy_Log` bits.

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
`position` does not reset between symbols, it simply iterates through
each position in the table, switching to the next symbol when enough
states have been allocated to the current one.

The result is a list of state values.
Each state will decode the current symbol.

To get the `Number_of_Bits` and `Baseline` required for next state,
it's first necessary to sort all states in their natural order.
The lower states will need 1 more bit than higher ones.

__Example__ :
Presuming a symbol has a probability of 5.
It receives 5 state values. States are sorted in natural order.

Next power of 2 is 8.
Space of probabilities is divided into 8 equal parts.
Presuming the `Accuracy_Log` is 7, it defines 128 states.
Divided by 8, each share is 16 large.

In order to reach 8, 8-5=3 lowest states will count "double",
taking shares twice larger,
requiring one more bit in the process.

Numbering starts from higher states using less bits.

| state order      |   0   |   1   |    2   |   3  |   4   |
| ---------------- | ----- | ----- | ------ | ---- | ----- |
| width            |  32   |  32   |   32   |  16  |  16   |
| `Number_of_Bits` |   5   |   5   |    5   |   4  |   4   |
| range number     |   2   |   4   |    6   |   0  |   1   |
| `Baseline`       |  32   |  64   |   96   |   0  |  16   |
| range            | 32-63 | 64-95 | 96-127 | 0-15 | 16-31 |

The next state is determined from current state
by reading the required `Number_of_Bits`, and adding the specified `Baseline`.


#### Bitstream

FSE bitstreams are read in reverse direction than written. In zstd,
the compressor writes bits forward into a block and the decompressor
must read the bitstream _backwards_.

To find the start of the bitstream it is therefore necessary to
know the offset of the last byte of the block which can be found
by counting `Block_Size` bytes after the block header.

After writing the last bit containing information, the compressor
writes a single `1`-bit and then fills the byte with 0-7 `0` bits of
padding. The last byte of the compressed bitstream cannot be `0` for
that reason.

When decompressing, the last byte containing the padding is the first
byte to read. The decompressor needs to skip 0-7 initial `0`-bits and
the first `1`-bit it occurs. Afterwards, the useful part of the bitstream
begins.

##### Starting states

The bitstream starts with initial state values,
each using the required number of bits in their respective _accuracy_,
decoded previously from their normalized distribution.

It starts by `Literals_Length_State`,
followed by `Offset_State`,
and finally `Match_Length_State`.

Reminder : always keep in mind that all values are read _backward_.

##### Decoding a sequence

A state gives a code.
A code provides `Baseline` and `Number_of_Bits` to add.
See [Symbol Decoding] section for details on each symbol.

Decoding starts by reading the `Number_of_Bits` required to decode `Offset`.
It then does the same for `Match_Length`,
and then for `Literals_Length`.

`Offset`, `Match_Length`, and `Literals_Length` define a sequence.
It starts by inserting the number of literals defined by `Literals_Length`,
then continue by copying `Match_Length` bytes from `currentPos - Offset`.

If it is not the last sequence in the block,
the next operation is to update states.
Using the rules pre-calculated in the decoding tables,
`Literals_Length_State` is updated,
followed by `Match_Length_State`,
and then `Offset_State`.

This operation will be repeated `Number_of_Sequences` times.
At the end, the bitstream shall be entirely consumed,
otherwise the bitstream is considered corrupted.

[Symbol Decoding]:#the-codes-for-literals-lengths-match-lengths-and-offsets

##### Repeat offsets

As seen in [Offset Codes], the first 3 values define a repeated offset and we will call them `Repeated_Offset1`, `Repeated_Offset2`, and `Repeated_Offset3`.
They are sorted in recency order, with `Repeated_Offset1` meaning "most recent one".

There is an exception though, when current sequence's literals length is `0`.
In this case, repeated offsets are shifted by one,
so `Repeated_Offset1` becomes `Repeated_Offset2`, `Repeated_Offset2` becomes `Repeated_Offset3`,
and `Repeated_Offset3` becomes `Repeated_Offset1 - 1_byte`.

In the first block, the offset history is populated with the following values : 1, 4 and 8 (in order).

Then each block gets its starting offset history from the ending values of the most recent compressed block.
Note that non-compressed blocks are skipped,
they do not contribute to offset history.

[Offset Codes]: #offset-codes

###### Offset updates rules

The newest offset takes the lead in offset history,
shifting others back (up to its previous place if it was already present).

This means that when `Repeated_Offset1` (most recent) is used, history is unmodified.
When `Repeated_Offset2` is used, it's swapped with `Repeated_Offset1`.
If any other offset is used, it becomes `Repeated_Offset1` and the rest are shift back by one.


Dictionary format
-----------------

`zstd` is compatible with "raw content" dictionaries, free of any format restriction,
except that they must be at least 8 bytes.
These dictionaries function as if they were just the `Content` block of a formatted
dictionary.

But dictionaries created by `zstd --train` follow a format, described here.

__Pre-requisites__ : a dictionary has a size,
                     defined either by a buffer limit, or a file size.

| `Magic_Number` | `Dictionary_ID` | `Entropy_Tables` | `Content` |
| -------------- | --------------- | ---------------- | --------- |

__`Magic_Number`__ : 4 bytes ID, value 0xEC30A437, little-endian format

__`Dictionary_ID`__ : 4 bytes, stored in little-endian format.
              `Dictionary_ID` can be any value, except 0 (which means no `Dictionary_ID`).
              It's used by decoders to check if they use the correct dictionary.

_Reserved ranges :_
              If the frame is going to be distributed in a private environment,
              any `Dictionary_ID` can be used.
              However, for public distribution of compressed frames,
              the following ranges are reserved for future use and should not be used :

              - low range : 1 - 32767
              - high range : >= (2^31)

__`Entropy_Tables`__ : following the same format as the tables in [compressed blocks].
              They are stored in following order :
              Huffman tables for literals, FSE table for offsets,
              FSE table for match lengths, and FSE table for literals lengths.
              It's finally followed by 3 offset values, populating recent offsets (instead of using `{1,4,8}`),
              stored in order, 4-bytes little-endian each, for a total of 12 bytes.
              Each recent offset must have a value < dictionary size.

__`Content`__ : The rest of the dictionary is its content.
              The content act as a "past" in front of data to compress or decompress,
              so it can be referenced in sequence commands.

[compressed blocks]: #the-format-of-compressed_block

Appendix A - Decoding tables for predefined codes
-------------------------------------------------

This appendix contains FSE decoding tables for the predefined literal length, match length, and offset
codes. The tables have been constructed using the algorithm as given above in the
"from normalized distribution to decoding tables" chapter. The tables here can be used as examples
to crosscheck that an implementation implements the decoding table generation algorithm correctly.

#### Literal Length Code:

| State | Symbol | Number_Of_Bits | Base |
| ----- | ------ | -------------- | ---- |
|     0 |      0 |              4 |    0 |
|     1 |      0 |              4 |   16 |
|     2 |      1 |              5 |   32 |
|     3 |      3 |              5 |    0 |
|     4 |      4 |              5 |    0 |
|     5 |      6 |              5 |    0 |
|     6 |      7 |              5 |    0 |
|     7 |      9 |              5 |    0 |
|     8 |     10 |              5 |    0 |
|     9 |     12 |              5 |    0 |
|    10 |     14 |              6 |    0 |
|    11 |     16 |              5 |    0 |
|    12 |     18 |              5 |    0 |
|    13 |     19 |              5 |    0 |
|    14 |     21 |              5 |    0 |
|    15 |     22 |              5 |    0 |
|    16 |     24 |              5 |    0 |
|    17 |     25 |              5 |   32 |
|    18 |     26 |              5 |    0 |
|    19 |     27 |              6 |    0 |
|    20 |     29 |              6 |    0 |
|    21 |     31 |              6 |    0 |
|    22 |      0 |              4 |   32 |
|    23 |      1 |              4 |    0 |
|    24 |      2 |              5 |    0 |
|    25 |      4 |              5 |   32 |
|    26 |      5 |              5 |    0 |
|    27 |      7 |              5 |   32 |
|    28 |      8 |              5 |    0 |
|    29 |     10 |              5 |   32 |
|    30 |     11 |              5 |    0 |
|    31 |     13 |              6 |    0 |
|    32 |     16 |              5 |   32 |
|    33 |     17 |              5 |    0 |
|    34 |     19 |              5 |   32 |
|    35 |     20 |              5 |    0 |
|    36 |     22 |              5 |   32 |
|    37 |     23 |              5 |    0 |
|    38 |     25 |              4 |    0 |
|    39 |     25 |              4 |   16 |
|    40 |     26 |              5 |   32 |
|    41 |     28 |              6 |    0 |
|    42 |     30 |              6 |    0 |
|    43 |      0 |              4 |   48 |
|    44 |      1 |              4 |   16 |
|    45 |      2 |              5 |   32 |
|    46 |      3 |              5 |   32 |
|    47 |      5 |              5 |   32 |
|    48 |      6 |              5 |   32 |
|    49 |      8 |              5 |   32 |
|    50 |      9 |              5 |   32 |
|    51 |     11 |              5 |   32 |
|    52 |     12 |              5 |   32 |
|    53 |     15 |              6 |    0 |
|    54 |     17 |              5 |   32 |
|    55 |     18 |              5 |   32 |
|    56 |     20 |              5 |   32 |
|    57 |     21 |              5 |   32 |
|    58 |     23 |              5 |   32 |
|    59 |     24 |              5 |   32 |
|    60 |     35 |              6 |    0 |
|    61 |     34 |              6 |    0 |
|    62 |     33 |              6 |    0 |
|    63 |     32 |              6 |    0 |

#### Match Length Code:

| State | Symbol | Number_Of_Bits | Base |
| ----- | ------ | -------------- | ---- |
|     0 |      0 |              6 |    0 |
|     1 |      1 |              4 |    0 |
|     2 |      2 |              5 |   32 |
|     3 |      3 |              5 |    0 |
|     4 |      5 |              5 |    0 |
|     5 |      6 |              5 |    0 |
|     6 |      8 |              5 |    0 |
|     7 |     10 |              6 |    0 |
|     8 |     13 |              6 |    0 |
|     9 |     16 |              6 |    0 |
|    10 |     19 |              6 |    0 |
|    11 |     22 |              6 |    0 |
|    12 |     25 |              6 |    0 |
|    13 |     28 |              6 |    0 |
|    14 |     31 |              6 |    0 |
|    15 |     33 |              6 |    0 |
|    16 |     35 |              6 |    0 |
|    17 |     37 |              6 |    0 |
|    18 |     39 |              6 |    0 |
|    19 |     41 |              6 |    0 |
|    20 |     43 |              6 |    0 |
|    21 |     45 |              6 |    0 |
|    22 |      1 |              4 |   16 |
|    23 |      2 |              4 |    0 |
|    24 |      3 |              5 |   32 |
|    25 |      4 |              5 |    0 |
|    26 |      6 |              5 |   32 |
|    27 |      7 |              5 |    0 |
|    28 |      9 |              6 |    0 |
|    29 |     12 |              6 |    0 |
|    30 |     15 |              6 |    0 |
|    31 |     18 |              6 |    0 |
|    32 |     21 |              6 |    0 |
|    33 |     24 |              6 |    0 |
|    34 |     27 |              6 |    0 |
|    35 |     30 |              6 |    0 |
|    36 |     32 |              6 |    0 |
|    37 |     34 |              6 |    0 |
|    38 |     36 |              6 |    0 |
|    39 |     38 |              6 |    0 |
|    40 |     40 |              6 |    0 |
|    41 |     42 |              6 |    0 |
|    42 |     44 |              6 |    0 |
|    43 |      1 |              4 |   32 |
|    44 |      1 |              4 |   48 |
|    45 |      2 |              4 |   16 |
|    46 |      4 |              5 |   32 |
|    47 |      5 |              5 |   32 |
|    48 |      7 |              5 |   32 |
|    49 |      8 |              5 |   32 |
|    50 |     11 |              6 |    0 |
|    51 |     14 |              6 |    0 |
|    52 |     17 |              6 |    0 |
|    53 |     20 |              6 |    0 |
|    54 |     23 |              6 |    0 |
|    55 |     26 |              6 |    0 |
|    56 |     29 |              6 |    0 |
|    57 |     52 |              6 |    0 |
|    58 |     51 |              6 |    0 |
|    59 |     50 |              6 |    0 |
|    60 |     49 |              6 |    0 |
|    61 |     48 |              6 |    0 |
|    62 |     47 |              6 |    0 |
|    63 |     46 |              6 |    0 |

#### Offset Code:

| State | Symbol | Number_Of_Bits | Base |
| ----- | ------ | -------------- | ---- |
|     0 |      0 |              5 |    0 |
|     1 |      6 |              4 |    0 |
|     2 |      9 |              5 |    0 |
|     3 |     15 |              5 |    0 |
|     4 |     21 |              5 |    0 |
|     5 |      3 |              5 |    0 |
|     6 |      7 |              4 |    0 |
|     7 |     12 |              5 |    0 |
|     8 |     18 |              5 |    0 |
|     9 |     23 |              5 |    0 |
|    10 |      5 |              5 |    0 |
|    11 |      8 |              4 |    0 |
|    12 |     14 |              5 |    0 |
|    13 |     20 |              5 |    0 |
|    14 |      2 |              5 |    0 |
|    15 |      7 |              4 |   16 |
|    16 |     11 |              5 |    0 |
|    17 |     17 |              5 |    0 |
|    18 |     22 |              5 |    0 |
|    19 |      4 |              5 |    0 |
|    20 |      8 |              4 |   16 |
|    21 |     13 |              5 |    0 |
|    22 |     19 |              5 |    0 |
|    23 |      1 |              5 |    0 |
|    24 |      6 |              4 |   16 |
|    25 |     10 |              5 |    0 |
|    26 |     16 |              5 |    0 |
|    27 |     28 |              5 |    0 |
|    28 |     27 |              5 |    0 |
|    29 |     26 |              5 |    0 |
|    30 |     25 |              5 |    0 |
|    31 |     24 |              5 |    0 |

Version changes
---------------
- 0.2.3 : clarified several details, by Sean Purcell
- 0.2.2 : added predefined codes, by Johannes Rudolph
- 0.2.1 : clarify field names, by Przemyslaw Skibinski
- 0.2.0 : numerous format adjustments for zstd v0.8
- 0.1.2 : limit Huffman tree depth to 11 bits
- 0.1.1 : reserved dictID ranges
- 0.1.0 : initial release
