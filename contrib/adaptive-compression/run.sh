make clean multi

echo "running file tests"

./multi tests/test2048.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test2048.pdf
echo "diff test complete: test2048.pdf"
rm tmp*

./multi tests/test512.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test512.pdf
echo "diff test complete: test512.pdf"
rm tmp*

./multi tests/test64.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test64.pdf
echo "diff test complete: test64.pdf"
rm tmp*

./multi tests/test16.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test16.pdf
echo "diff test complete: test16.pdf"
rm tmp*

./multi tests/test4.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test4.pdf
echo "diff test complete: test4.pdf"
rm tmp*

./multi tests/test.pdf -otmp.zst
zstd -d tmp.zst
diff tmp tests/test.pdf
echo "diff test complete: test.pdf"
rm tmp*

echo "Running std input/output tests"

cat tests/test2048.pdf | ./multi -otmp.zst
zstd -d tmp.zst
diff tmp tests/test2048.pdf
echo "diff test complete: test2048.pdf"
rm tmp*

cat tests/test512.pdf | ./multi -otmp.zst
zstd -d tmp.zst
diff tmp tests/test512.pdf
echo "diff test complete: test512.pdf"
rm tmp*

cat tests/test64.pdf | ./multi -otmp.zst
zstd -d tmp.zst
diff tmp tests/test64.pdf
echo "diff test complete: test64.pdf"
rm tmp*

cat tests/test16.pdf | ./multi -otmp.zst
zstd -d tmp.zst
diff tmp tests/test16.pdf
echo "diff test complete: test16.pdf"
rm tmp*

cat tests/test4.pdf | ./multi -otmp.zst
zstd -d tmp.zst
diff tmp tests/test4.pdf
echo "diff test complete: test4.pdf"
rm tmp*

cat tests/test.pdf | ./multi -otmp.zst
zstd -d tmp.zst
diff tmp tests/test.pdf
echo "diff test complete: test.pdf"
rm tmp*

make clean
