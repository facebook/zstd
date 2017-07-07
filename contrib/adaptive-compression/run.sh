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

cat tests/test2048.pdf | ./multi > tmp.zst
zstd -d tmp.zst
diff tmp tests/test2048.pdf
echo "diff test complete: test2048.pdf"
rm tmp*

cat tests/test512.pdf | ./multi > tmp.zst
zstd -d tmp.zst
diff tmp tests/test512.pdf
echo "diff test complete: test512.pdf"
rm tmp*

cat tests/test64.pdf | ./multi > tmp.zst
zstd -d tmp.zst
diff tmp tests/test64.pdf
echo "diff test complete: test64.pdf"
rm tmp*

cat tests/test16.pdf | ./multi > tmp.zst
zstd -d tmp.zst
diff tmp tests/test16.pdf
echo "diff test complete: test16.pdf"
rm tmp*

cat tests/test4.pdf | ./multi > tmp.zst
zstd -d tmp.zst
diff tmp tests/test4.pdf
echo "diff test complete: test4.pdf"
rm tmp*

cat tests/test.pdf | ./multi > tmp.zst
zstd -d tmp.zst
diff tmp tests/test.pdf
echo "diff test complete: test.pdf"
rm tmp*

echo "Running multi-file tests"
./multi tests/*
zstd -d tests/test.pdf.zst -o tests/tmp
zstd -d tests/test2.pdf.zst -o tests/tmp2
zstd -d tests/test4.pdf.zst -o tests/tmp4
zstd -d tests/test8.pdf.zst -o tests/tmp8
zstd -d tests/test16.pdf.zst -o tests/tmp16
zstd -d tests/test32.pdf.zst -o tests/tmp32
zstd -d tests/test64.pdf.zst -o tests/tmp64
zstd -d tests/test128.pdf.zst -o tests/tmp128
zstd -d tests/test256.pdf.zst -o tests/tmp256
zstd -d tests/test512.pdf.zst -o tests/tmp512
zstd -d tests/test1024.pdf.zst -o tests/tmp1024
zstd -d tests/test2048.pdf.zst -o tests/tmp2048

diff tests/test.pdf tests/tmp
diff tests/test2.pdf tests/tmp2
diff tests/test4.pdf tests/tmp4
diff tests/test8.pdf tests/tmp8
diff tests/test16.pdf tests/tmp16
diff tests/test32.pdf tests/tmp32
diff tests/test64.pdf tests/tmp64
diff tests/test128.pdf tests/tmp128
diff tests/test256.pdf tests/tmp256
diff tests/test512.pdf tests/tmp512
diff tests/test1024.pdf tests/tmp1024
diff tests/test2048.pdf tests/tmp2048

rm -f tests/*.zst tests/tmp*
echo "Running Args Tests"
./multi -h
./multi -i22 -p -s -otmp.zst tests/test2048.pdf
rm tmp*

echo "Running Tests With Multiple Files > stdout"
./multi tests/* -c > tmp.zst
zstd -d tmp.zst
rm tmp*

echo "finished with tests"

make clean
