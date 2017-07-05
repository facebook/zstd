make clean multi

./multi tests/test2048.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test2048.pdf
echo "diff test complete"
rm tmp*

./multi tests/test512.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test512.pdf
echo "diff test complete"
rm tmp*

./multi tests/test64.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test64.pdf
echo "diff test complete"
rm tmp*

./multi tests/test16.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test16.pdf
echo "diff test complete"
rm tmp*

./multi tests/test4.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test4.pdf
echo "diff test complete"
rm tmp*

./multi tests/test.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test.pdf
echo "diff test complete"
rm tmp*

make clean
