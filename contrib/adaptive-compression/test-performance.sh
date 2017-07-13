echo "testing time"
./datagen -g1GB > tmp
time ./adapt -otmp1.zst tmp
time zstd -1 -o tmp2.zst tmp
rm tmp*

./datagen -g2GB > tmp
time ./adapt -otmp1.zst tmp
time zstd -1 -o tmp2.zst tmp
rm tmp*

./datagen -g4GB > tmp
time ./adapt -otmp1.zst tmp
time zstd -1 -o tmp2.zst tmp
rm tmp*

echo -e "\ntesting compression ratio"
./datagen -g1GB > tmp
time ./adapt -otmp1.zst tmp
time zstd -1 -o tmp2.zst tmp
ls -l tmp1.zst tmp2.zst
rm tmp*

./datagen -g2GB > tmp
time ./adapt -otmp1.zst tmp
time zstd -1 -o tmp2.zst tmp
ls -l tmp1.zst tmp2.zst
rm tmp*

./datagen -g4GB > tmp
time ./adapt -otmp1.zst tmp
time zstd -1 -o tmp2.zst tmp
ls -l tmp1.zst tmp2.zst
rm tmp*
