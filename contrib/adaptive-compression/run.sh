make clean multi
./multi tests/test2048.pdf tmp.zst
zstd -d tmp.zst
diff tmp tests/test2048.pdf
echo "diff test complete"
make clean
