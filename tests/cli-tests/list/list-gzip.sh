#!/bin/sh
set -e

# Deterministic gzip file:
# content "hello world, hello world, hello world\n", no flags, mtime=0, OS=255
printf '\037\213\010\000\000\000\000\000\000\377\313\110\315\311\311\127\050\317\057\312\111\321\121\310\300\301\341\002\000\176\371\230\254\046\000\000\000' > hello.gz

# List a gzip file
zstd -l hello.gz

# Verbose listing
zstd -lv hello.gz

# Multiple gzip files show a CRC32 total row
cp hello.gz world.gz
zstd -l hello.gz world.gz

# Listing files not compressed by zstd or gzip still fails
println "plain text" > plain.txt
zstd -l plain.txt && die "listing an uncompressed file should fail"

# Listing a truncated gzip file fails
head -c 5 hello.gz > truncated.gz
zstd -l truncated.gz && die "listing a truncated gzip file should fail"

exit 0
