#!/bin/sh

println "+ zstd --blah" >&2
zstd --blah
println "+ zstd -xz" >&2
zstd -xz
println "+ zstd --adapt=min=1,maxx=2 file.txt" >&2
zstd --adapt=min=1,maxx=2 file.txt
println "+ zstd --train-cover=k=48,d=8,steps32 file.txt" >&2
zstd --train-cover=k=48,d=8,steps32 file.txt
