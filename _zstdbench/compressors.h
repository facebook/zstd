#include <stdlib.h> 
#include <stdint.h> // int64_t

typedef int64_t (*compress_func)(char *in, size_t insize, char *out, size_t outsize, size_t, size_t, size_t);



#ifndef BENCH_REMOVE_ZSTD
	int64_t lzbench_zstd_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, size_t);
	int64_t lzbench_zstd_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, size_t);
#else
	#define lzbench_zstd_compress NULL
	#define lzbench_zstd_decompress NULL
#endif


#ifndef BENCH_REMOVE_ZSTDHC
	int64_t lzbench_zstdhc_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, size_t);
	int64_t lzbench_zstdhc_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, size_t);
#else
	#define lzbench_zstdhc_compress NULL
	#define lzbench_zstdhc_decompress NULL
#endif

