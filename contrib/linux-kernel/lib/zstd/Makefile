obj-$(CONFIG_ZSTD_COMPRESS) += zstd_compress.o
obj-$(CONFIG_ZSTD_DECOMPRESS) += zstd_decompress.o

ccflags-y += -O3

zstd_compress-y := entropy_common.o fse_decompress.o xxhash.o zstd_common.o \
				fse_compress.o huf_compress.o compress.o
zstd_decompress-y := entropy_common.o fse_decompress.o xxhash.o zstd_common.o \
				huf_decompress.o decompress.o
