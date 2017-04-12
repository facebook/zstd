#ifndef SEEKABLE_H
#define SEEKABLE_H

#if defined (__cplusplus)
extern "C" {
#endif

static const unsigned ZSTD_seekTableFooterSize = 9;

#define ZSTD_SEEKABLE_MAGICNUMBER 0x8F92EAB1

#define ZSTD_SEEKABLE_MAXFRAMES 0x8000000U

/* Limit the maximum size to avoid any potential issues storing the compressed size */
#define ZSTD_SEEKABLE_MAX_FRAME_DECOMPRESSED_SIZE 0x80000000U

/*-****************************************************************************
*  Seekable Format
*
*  The seekable format splits the compressed data into a series of "frames",
*  each compressed individually so that decompression of a section in the
*  middle of an archive only requires zstd to decompress at most a frame's
*  worth of extra data, instead of the entire archive.
******************************************************************************/

typedef struct ZSTD_seekable_CStream_s ZSTD_seekable_CStream;
typedef struct ZSTD_seekable_DStream_s ZSTD_seekable_DStream;

/*-****************************************************************************
*  Seekable compression - HowTo
*  A ZSTD_seekable_CStream object is required to tracking streaming operation.
*  Use ZSTD_seekable_createCStream() and ZSTD_seekable_freeCStream() to create/
*  release resources.
*
*  Streaming objects are reusable to avoid allocation and deallocation,
*  to start a new compression operation call ZSTD_seekable_initCStream() on the
*  compressor.
*
*  Data streamed to the seekable compressor will automatically be split into
*  frames of size `maxFrameSize` (provided in ZSTD_seekable_initCStream()),
*  or if none is provided, will be cut off whenver ZSTD_endFrame() is called
*  or when the default maximum frame size is reached (approximately 4GB).
*
*  Use ZSTD_seekable_initCStream() to initialize a ZSTD_seekable_CStream object
*  for a new compression operation.
*  `maxFrameSize` indicates the size at which to automatically start a new
*  seekable frame.  `maxFrameSize == 0` implies the default maximum size.
*  `checksumFlag` indicates whether or not the seek table should include frame
*  checksums on the uncompressed data for verification.
*  @return : a size hint for input to provide for compression, or an error code
*            checkable with ZSTD_isError()
*
*  Use ZSTD_seekable_compressStream() repetitively to consume input stream.
*  The function will automatically update both `pos` fields.
*  Note that it may not consume the entire input, in which case `pos < size`,
*  and it's up to the caller to present again remaining data.
*  @return : a size hint, preferred nb of bytes to use as input for next
*            function call or an error code, which can be tested using
*            ZSTD_isError().
*            Note 1 : it's just a hint, to help latency a little, any other
*                     value will work fine.
*            Note 2 : size hint is guaranteed to be <= ZSTD_CStreamInSize()
*
*  At any time, call ZSTD_seekable_endFrame() to end the current frame and
*  start a new one.
*
*  ZSTD_seekable_endStream() will end the current frame, and then write the seek
*  table so that decompressors can efficiently find compressed frames.
*  ZSTD_seekable_endStream() may return a number > 0 if it was unable to flush
*  all the necessary data to `output`.  In this case, it should be called again
*  until all remaining data is flushed out and 0 is returned.
******************************************************************************/

/*===== Seekable compressor management =====*/
ZSTDLIB_API ZSTD_seekable_CStream* ZSTD_seekable_createCStream(void);
ZSTDLIB_API size_t ZSTD_seekable_freeCStream(ZSTD_seekable_CStream* zcs);

/*===== Seekable compression functions =====*/
ZSTDLIB_API size_t ZSTD_seekable_initCStream(ZSTD_seekable_CStream* zcs, int compressionLevel, int checksumFlag, unsigned maxFrameSize);
ZSTDLIB_API size_t ZSTD_seekable_compressStream(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output, ZSTD_inBuffer* input);
ZSTDLIB_API size_t ZSTD_seekable_endFrame(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output);
ZSTDLIB_API size_t ZSTD_seekable_endStream(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output);

/*-****************************************************************************
*  Seekable decompression - HowTo
*  A ZSTD_seekable_DStream object is required to tracking streaming operation.
*  Use ZSTD_seekable_createDStream() and ZSTD_seekable_freeDStream() to create/
*  release resources.
*
*  Streaming objects are reusable to avoid allocation and deallocation,
*  to start a new compression operation call ZSTD_seekable_initDStream() on the
*  compressor.
*
*  Use ZSTD_seekable_loadSeekTable() to load the seek table from a file.
*  `src` should point to a block of data read from the end of the file,
*  i.e. `src + srcSize` should always be the end of the file.
*  @return : 0 if the table was loaded successfully, or if `srcSize` was too
*            small, a size hint for how much data to provide.
*            An error code may also be returned, checkable with ZSTD_isError()
*
*  Use ZSTD_initDStream to prepare for a new decompression operation using the
*  seektable loaded with ZSTD_seekable_loadSeekTable().
*  Data in the range [rangeStart, rangeEnd) will be decompressed.
*
*  Call ZSTD_seekable_decompressStream() repetitively to consume input stream.
*  @return : There are a number of possible return codes for this function
*           - 0, the decompression operation has completed.
*           - An error code checkable with ZSTD_isError
*             + If this error code is ZSTD_error_needSeek, the user should seek
*               to the file position provided by ZSTD_seekable_getSeekOffset()
*               and indicate this to the stream with
*               ZSTD_seekable_updateOffset(), before resuming decompression
*             + Otherwise, this is a regular decompression error and the input
*               file is likely corrupted or the API was incorrectly used.
*           - A size hint, the preferred nb of bytes to provide as input to the
*             next function call to improve latency.
*
*  ZSTD_seekable_getSeekOffset() and ZSTD_seekable_updateOffset() are helper
*  functions to indicate where the user should seek their file stream to, when
*  a different position is required to continue decompression.
*  Note that ZSTD_seekable_updateOffset will error if given an offset other
*  than the one requested from ZSTD_seekable_getSeekOffset().
******************************************************************************/

/*===== Seekable decompressor management =====*/
ZSTDLIB_API ZSTD_seekable_DStream* ZSTD_seekable_createDStream(void);
ZSTDLIB_API size_t ZSTD_seekable_freeDStream(ZSTD_seekable_DStream* zds);

/*===== Seekable decompression functions =====*/
ZSTDLIB_API size_t ZSTD_seekable_loadSeekTable(ZSTD_seekable_DStream* zds, const void* src, size_t srcSize);
ZSTDLIB_API size_t ZSTD_seekable_initDStream(ZSTD_seekable_DStream* zds, unsigned long long rangeStart, unsigned long long rangeEnd);
ZSTDLIB_API size_t ZSTD_seekable_decompressStream(ZSTD_seekable_DStream* zds, ZSTD_outBuffer* output, ZSTD_inBuffer* input);
ZSTDLIB_API unsigned long long ZSTD_seekable_getSeekOffset(ZSTD_seekable_DStream* zds);
ZSTDLIB_API size_t ZSTD_seekable_updateOffset(ZSTD_seekable_DStream* zds, unsigned long long offset);

#if defined (__cplusplus)
}
#endif

#endif
