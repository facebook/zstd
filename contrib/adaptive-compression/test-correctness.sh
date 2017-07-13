echo "correctness tests -- general"
./datagen -g1GB > tmp
./adapt -otmp.zst tmp
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g500MB > tmp
./adapt -otmp.zst tmp
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g250MB > tmp
./adapt -otmp.zst tmp
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g125MB > tmp
./adapt -otmp.zst tmp
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g50MB > tmp
./adapt -otmp.zst tmp
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g25MB > tmp
./adapt -otmp.zst tmp
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g10MB > tmp
./adapt -otmp.zst tmp
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g5MB > tmp
./adapt -otmp.zst tmp
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g500KB > tmp
./adapt -otmp.zst tmp
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

echo -e "\ncorrectness tests -- streaming"
./datagen -g1GB > tmp
cat tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g100MB > tmp
cat tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g10MB > tmp
cat tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g1MB > tmp
cat tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g100KB > tmp
cat tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g10KB > tmp
cat tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

echo -e "\ncorrectness tests -- read limit"
./datagen -g1GB > tmp
pv -L 50m -q tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g100MB > tmp
pv -L 50m -q tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g10MB > tmp
pv -L 50m -q tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g1MB > tmp
pv -L 50m -q tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g100KB > tmp
pv -L 50m -q tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g10KB > tmp
pv -L 50m -q tmp | ./adapt > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

echo -e "\ncorrectness tests -- write limit"
./datagen -g1GB > tmp
pv -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g100MB > tmp
pv -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g10MB > tmp
pv -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g1MB > tmp
pv -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g100KB > tmp
pv -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g10KB > tmp
pv -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

echo -e "\ncorrectness tests -- read and write limits"
./datagen -g1GB > tmp
pv -L 50m -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g100MB > tmp
pv -L 50m -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g10MB > tmp
pv -L 50m -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g1MB > tmp
pv -L 50m -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g100KB > tmp
pv -L 50m -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*

./datagen -g10KB > tmp
pv -L 50m -q tmp | ./adapt | pv -L 5m -q > tmp.zst
zstd -d tmp.zst -o tmp2
diff -q tmp tmp2
rm tmp*


make clean
