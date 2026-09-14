# !/bin/sh
set -e

# Benchmarks run on a Ubuntu 22.04 VM with 2 cores and 8 GiB of RAM.
# The VM is c6i.xlarge instance with a Intel 8375C 2.90GHz processor and gp3 SSD with throughput 500 MB/s.
# Hyperthreading disabled
# Kernel: 6.2

# silesia is a directory that can be downloaded from
# http://mattmahoney.net/dc/silesia.html
# $ ls -lh ./silesia/
# total 203M
# -rw-rw-r-- 1 ubuntu ubuntu 9.8M Apr 12  2002 dickens
# -rw-rw-r-- 1 ubuntu ubuntu  49M May 31  2002 mozilla
# -rw-rw-r-- 1 ubuntu ubuntu 9.6M Mar 20  2003 mr
# -rw-rw-r-- 1 ubuntu ubuntu  32M Apr  2  2002 nci
# -rw-rw-r-- 1 ubuntu ubuntu 5.9M Jul  4  2002 ooffice
# -rw-rw-r-- 1 ubuntu ubuntu 9.7M Apr 11  2002 osdb
# -rw-rw-r-- 1 ubuntu ubuntu 6.4M Apr  2  2002 reymont
# -rw-rw-r-- 1 ubuntu ubuntu  21M Mar 25  2002 samba
# -rw-rw-r-- 1 ubuntu ubuntu 7.0M Mar 24  2002 sao
# -rw-rw-r-- 1 ubuntu ubuntu  40M Mar 25  2002 webster
# -rw-rw-r-- 1 ubuntu ubuntu 8.1M Apr  4  2002 x-ray
# -rw-rw-r-- 1 ubuntu ubuntu 5.1M Nov 30  2000 xml

# $HOME is on a ext4 filesystem
BENCHMARK_DIR="$HOME/silesia/"
N=10

# Normalize the environment
sudo umount /mnt/btrfs 2>/dev/null >/dev/null || true
sudo mount -t btrfs $@ /dev/nvme1n1 /mnt/btrfs
sudo rm -rf /mnt/btrfs/*
sync
sudo umount /mnt/btrfs
sudo mount -t btrfs $@ /dev/nvme1n1 /mnt/btrfs

# Run the benchmark
echo "Compression"
time sh -c "for i in \$(seq $N); do sudo cp -r $BENCHMARK_DIR /mnt/btrfs/\$i; done; sync"

echo "Approximate compression ratio"
printf "print(1 / (%d / %d))\n" \
  $(df /mnt/btrfs --output=used -B 1 | tail -n 1) \
  $(sudo du /mnt/btrfs -b -d 0 | tr '\t' '\n' | head -n 1) | python3 -

# Unmount and remount to avoid any caching
sudo umount /mnt/btrfs
sudo mount -t btrfs $@ /dev/nvme1n1 /mnt/btrfs

echo "Decompression"
time sudo tar -c /mnt/btrfs 2>/dev/null | wc -c >/dev/null

sudo rm -rf /mnt/btrfs/*
sudo umount /mnt/btrfs

# Run for each of -o compress-force={none, lzo, zlib, zstd} 5 times and take the
# min time and ratio.
# Ran zstd with compression levels {1, 3, 6, 9, 12, 15}.
# Original size: 2119415342 B (using du /mnt/btrfs)

# none
# compress: 4.205 s
# decompress: 3.808 s
# ratio: 0.99

# lzo
# compress: 3.021 s
# decompress: 8.162 s
# ratio: 1.68

# zlib 3
# compress: 23.656 s
# decompress: 17.454 s
# ratio : 2.64

# zstd 1
# compress: 4.502 s
# decompress: 9.569 s
# ratio : 2.64

# zstd 3
# compress: 6.225 s
# decompress: 9.576 s
# ratio : 2.78

# zstd 6
# compress: 13.551 s
# decompress: 9.632 s
# ratio : 2.94

# zstd 9
# compress: 23.065 s
# decompress: 10.379 s
# ratio : 2.99

# zstd 12
# compress: 62.375 s
# decompress: 11.503 s
# ratio : 3.00

# zstd 15
# compress: 133.582 s
# decompress: 12.124 s
# ratio : 3.14
