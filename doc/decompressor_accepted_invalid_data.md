Decompressor Accepted Invalid Data
==================================

This document describes the behavior of the reference decompressor in cases
where it accepts an invalid frame instead of reporting an error.

Zero offsets converted to 1
---------------------------
If a sequence is decoded with `literals_length = 0` and `offset_value = 3`
while `Repeated_Offset_1 = 1`, the computed offset will be `0`, which is
invalid.

The reference decompressor will process this case as if the computed
offset was `1`, including inserting `1` into the repeated offset list.