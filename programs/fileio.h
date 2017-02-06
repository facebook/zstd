/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


#ifndef FILEIO_H_23981798732
#define FILEIO_H_23981798732

#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_compressionParameters */
#include "zstd.h"                  /* ZSTD_* */

#if defined (__cplusplus)
extern "C" {
#endif


/* *************************************
*  Special i/o constants
**************************************/
#define stdinmark  "/*stdin*\\"
#define stdoutmark "/*stdout*\\"
#ifdef _WIN32
#  define nulmark "nul"
#else
#  define nulmark "/dev/null"
#endif


/*-*************************************
*  Parameters
***************************************/
void FIO_overwriteMode(void);
void FIO_setNotificationLevel(unsigned level);
void FIO_setSparseWrite(unsigned sparse);  /**< 0: no sparse; 1: disable on stdout; 2: always enabled */
void FIO_setDictIDFlag(unsigned dictIDFlag);
void FIO_setChecksumFlag(unsigned checksumFlag);
void FIO_setRemoveSrcFile(unsigned flag);
void FIO_setMemLimit(unsigned memLimit);
void FIO_setNbThreads(unsigned nbThreads);
void FIO_setBlockSize(unsigned blockSize);
void FIO_setOverlapLog(unsigned overlapLog);


/*-*************************************
*  Single File functions
***************************************/
/** FIO_compressFilename() :
    @return : 0 == ok;  1 == pb with src file. */
int FIO_compressFilename (const char* outfilename, const char* infilename, const char* dictFileName,
                          int compressionLevel, ZSTD_compressionParameters* comprParams);

/** FIO_decompressFilename() :
    @return : 0 == ok;  1 == pb with src file. */
int FIO_decompressFilename (const char* outfilename, const char* infilename, const char* dictFileName);


/*-*************************************
*  Multiple File functions
***************************************/
/** FIO_compressMultipleFilenames() :
    @return : nb of missing files */
int FIO_compressMultipleFilenames(const char** srcNamesTable, unsigned nbFiles,
                                  const char* suffix,
                                  const char* dictFileName, int compressionLevel,
                                  ZSTD_compressionParameters* comprParams);

/** FIO_decompressMultipleFilenames() :
    @return : nb of missing or skipped files */
int FIO_decompressMultipleFilenames(const char** srcNamesTable, unsigned nbFiles,
                                    const char* suffix,
                                    const char* dictFileName);


#if defined (__cplusplus)
}
#endif

#endif  /* FILEIO_H_23981798732 */
