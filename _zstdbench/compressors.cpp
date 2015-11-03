#include "compressors.h"
#include <stdio.h>
#include <stdint.h>



#ifndef BENCH_REMOVE_ZSTD
#include "../lib/zstd.h"

int64_t lzbench_zstd_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t level, size_t, size_t)
{
	return ZSTD_compress(outbuf, outsize, inbuf, insize);
}

int64_t lzbench_zstd_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, size_t)
{
	return ZSTD_decompress(outbuf, outsize, inbuf, insize);
}

#endif




#ifndef BENCH_REMOVE_ZSTDHC
#include "../lib/zstdhc.h"

int64_t lzbench_zstdhc_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t level, size_t, size_t)
{
	return ZSTD_HC_compress(outbuf, outsize, inbuf, insize, level);
}

int64_t lzbench_zstdhc_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, size_t)
{
	return ZSTD_decompress(outbuf, outsize, inbuf, insize);
}

#endif



