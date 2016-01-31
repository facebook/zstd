#include "zstd.h"     /* ZSTD version numbers */

#if defined (__cplusplus)
extern "C" {
#endif

	int ZStdCompress(int dstPtr, int _maxDstSize, int srcPtr, int _srcSize, int compressionLevel) {
		// size_t ZSTD_compress(void* dst, size_t maxDstSize, const void* src, size_t srcSize, int compressionLevel)
		void *dst = (void*)dstPtr;
		size_t maxDstSize = (size_t)_maxDstSize;
		void *src = (void*)srcPtr;
		size_t srcSize = (size_t)_srcSize;

		size_t compressedSize = ZSTD_compress(dst, maxDstSize, src, srcSize, compressionLevel);
		return (int)compressedSize;
	}

	int ZStdDecompress(int dstPtr, int _maxDstSize, int srcPtr, int _srcSize) {
		// size_t ZSTD_decompress(void* dst, size_t maxDstSize, const void* src, size_t srcSize)
		void *dst = (void*)dstPtr;
		size_t maxDstSize = (size_t)_maxDstSize;
		void *src = (void*)srcPtr;
		size_t srcSize = (size_t)_srcSize;

		size_t decompressedSize = ZSTD_decompress(dst, maxDstSize, src, srcSize);
		return (int)decompressedSize;
	}

#if defined (__cplusplus)
}
#endif