# Zstandard Dictionary-in-Stream Format

### Version

0.1.0 (2020-10-11): initial version

## Introduction

This document defines a format for including a Zstandard dictionary inside a
compressed Zstandard stream. When combined with the
[Seekable Format](../seekable_format) or other formats that use multiple
Zstandard frames, this format can help reduce the size of each frame without
requiring an external dictionary. This format can also be used to create a
stand-alone stream from a stream that uses an external dictionary, without
needing to recompress the stream.

### Usage

This format is used by tools such as [Wget-AT] to compress WARC files.

[Wget-AT]: https://github.com/ArchiveTeam/wget-lua/releases/tag/v1.20.3-at.20200401.01

## Format

The format consists of a skippable frame containing the dictionary, followed by
a normal Zstandard stream. All compressed frames in the stream must have been
compressed using the dictionary.

### Dictionary Frame Format

The dictionary frame is a [Zstandard skippable frame], structured as follows:

|`Magic_Number`|`Frame_Size`|`Compressed_or_Uncompressed_Dictionary`   |
|--------------|------------|------------------------------------------|
| 4 bytes      | 4 bytes    | n bytes                                  |

__`Magic_Number`__

Little-endian value: 0x184D2A5D.
Since it is legal for other Zstandard skippable frames to use the same
magic number, it is not recommended for a decoder to recognize frames
solely on this.

__`Frame_Size`__

Little-endian, the total size of the skippable frame, not including the
`Magic_Number` or `Frame_Size`.

__`Compressed_or_Uncompressed_Dictionary`__

The dictionary data, which may optionally be compressed.

If uncompressed, this data must conform to the [Dictionary Format]. In
particular, it must start with little-endian 0xEC30A437.

Otherwise, this data must be a single Zstandard compressed frame that
decompresses into data in the Dictionary Format. In particular, the compressed
data must start with little-endian 0xFD2FB528. The frame __must__ include a
`Frame_Content_Size` field. This field __must not__ contain any skippable
frames.

[Dictionary Format]: https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md#dictionary-format
[Zstandard skippable frame]: https://github.com/facebook/zstd/blob/master/doc/zstd_compression_format.md#skippable-frames
