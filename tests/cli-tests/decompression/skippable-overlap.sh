#!/bin/sh

set -eu

# An 8-byte empty skippable frame followed by a 9-byte empty zstd frame.
# The initial 18-byte header refill moves this 17-byte stream into the
# coalesce buffer. After consuming the skippable frame, the next refill
# moves 9 bytes from offset 8 back to offset 0. Run under ASan to detect
# an overlapping memcpy.
printf '\120\052\115\030\000\000\000\000\050\265\057\375\040\000\001\000\000' > skippable-overlap.zst

# This is a valid stream and must decode successfully to empty output.
for io in --asyncio --no-asyncio; do
    for operation in -t -dc; do
        zstd -q "$io" "$operation" skippable-overlap.zst
    done
done
