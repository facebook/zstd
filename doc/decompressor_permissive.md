Decompressor Permissiveness to Invalid Data
===========================================

This document describes the behavior of the reference decompressor in cases
where it accepts formally invalid data instead of reporting an error.

While the reference decompressor *must* decode any compliant frame following
the specification, its ability to detect erroneous data is on a best effort
basis: the decoder may accept input data that would be formally invalid,
when it causes no risk to the decoder, and which detection would cost too much
complexity or speed regression.

In practice, the vast majority of invalid data are detected, if only because
many corruption events are dangerous for the decoder process (such as
requesting an out-of-bound memory access) and many more are easy to check.

This document lists a few known cases where invalid data was formerly accepted
by the decoder, and what has changed since.


Truncated Huffman states
------------------------

**Last affected version**: v1.5.6

**Produced by the reference compressor**: No

**Example Frame**: `28b5 2ffd 0000 5500 0072 8001 0420 7e1f 02aa 00`

When using FSE-compressed Huffman weights, the compressed weight bitstream
could contain fewer bits than necessary to decode the initial states.

The reference decompressor up to v1.5.6 will decode truncated or missing
initial states as zero, which can result in a valid Huffman tree if only
the second state is truncated.

In newer versions, truncated initial states are reported as a corruption
error by the decoder.


Offset == 0
-----------

**Last affected version**: v1.5.5

**Produced by the reference compressor**: No

**Example Frame**: `28b5 2ffd 0000 4500 0008 0002 002f 430b ae`

If a sequence is decoded with `literals_length = 0` and `offset_value = 3`
while `Repeated_Offset_1 = 1`, the computed offset will be `0`, which is
invalid.

The reference decompressor up to v1.5.5 processes this case as if the computed
offset was `1`, including inserting `1` into the repeated offset list.
This prevents the output buffer from remaining uninitialized, thus denying a
potential attack vector from an untrusted source.
However, in the rare case where this scenario would be the outcome of a
transmission or storage error, the decoder relies on the checksum to detect
the error.

In newer versions, this case is always detected and reported as a corruption error.


Non-zeroes reserved bits
------------------------

**Last affected version**: v1.5.5

**Produced by the reference compressor**: No

The Sequences section of each block has a header, and one of its elements is a
byte, which describes the compression mode of each symbol.
This byte contains 2 reserved bits which must be set to zero.

The reference decompressor up to v1.5.5 just ignores these 2 bits.
This behavior has no consequence for the rest of the frame decoding process.

In newer versions, the 2 reserved bits are actively checked for value zero,
and the decoder reports a corruption error if they are not.
