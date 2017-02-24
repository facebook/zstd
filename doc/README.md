Zstandard Documentation
=======================

This directory contains material defining the Zstandard format,
as well as for help using the `zstd` library.

__`zstd_compression_format.md`__ : This document defines the Zstandard compression format.
Compliant decoders must adhere to this document,
and compliant encoders must generate data that follows it.

__`educational_decoder`__ : This directory contains an implementation of a Zstandard decoder,
compliant with the Zstandard compression format.
It can be used, for example, to better understand the format,
or as the basis for a separate implementation a Zstandard decoder/encoder.

__`zstd_manual.html`__ : Documentation on the functions found in `zstd.h`.
See [http://zstd.net/zstd_manual.html](http://zstd.net/zstd_manual.html) for
the manual released with the latest official `zstd` release.


