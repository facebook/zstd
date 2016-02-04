/*
    dictBuilder.h
    Copyright (C) Yann Collet 2016

    GPL v2 License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    You can contact the author at :
    - zstd source repository : https://github.com/Cyan4973/zstd
    - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* This library is designed for a single-threaded console application.
*  It exit() and printf() into stderr when it encounters an error condition. */

#ifndef DICTBUILDER_H_001
#define DICTBUILDER_H_001

/*-*************************************
*  Version
***************************************/
#define DiB_VERSION_MAJOR    0    /* for breaking interface changes  */
#define DiB_VERSION_MINOR    0    /* for new (non-breaking) interface capabilities */
#define DiB_VERSION_RELEASE  1    /* for tweaks, bug-fixes, or development */
#define DiB_VERSION_NUMBER  (DiB_VERSION_MAJOR *100*100 + DiB_VERSION_MINOR *100 + DiB_VERSION_RELEASE)
unsigned DiB_versionNumber (void);


/*-*************************************
*  Public type
***************************************/
typedef struct {
    unsigned selectivityLevel;   /* 0 means default; larger => bigger selection => larger dictionary */
    unsigned compressionLevel;   /* 0 means default; target a specific zstd compression level */
} DiB_params_t;


/*-*************************************
*  Public functions
***************************************/
/*! DiB_trainFromBuffer
    Train a dictionary from a memory buffer @samplesBuffer
    where @nbSamples samples have been stored concatenated.
    Each sample size is provided into an orderly table @sampleSizes.
    Resulting dictionary will be saved into @dictBuffer.
    @parameters is optional and can be provided with 0 values to mean "default".
    @result : size of dictionary stored into @dictBuffer (<= @dictBufferSize)
              or an error code, which can be tested by DiB_isError().
    note : DiB_trainFromBuffer() will send notifications into stderr if instructed to, using DiB_setNotificationLevel()
*/
size_t DiB_trainFromBuffer(void* dictBuffer, size_t dictBufferSize,
                           const void* samplesBuffer, const size_t* sampleSizes, unsigned nbSamples,
                           DiB_params_t parameters);


/*! DiB_trainFromFiles
    Train a dictionary from a set of files provided by @fileNamesTable
    Resulting dictionary is written into file @dictFileName.
    @parameters is optional and can be provided with 0 values.
    @result : 0 == ok. Any other : error.
*/
int DiB_trainFromFiles(const char* dictFileName, unsigned maxDictSize,
                       const char** fileNamesTable, unsigned nbFiles,
                       DiB_params_t parameters);


/*-*************************************
*  Helper functions
***************************************/
unsigned DiB_isError(size_t errorCode);
const char* DiB_getErrorName(size_t errorCode);

/*! DiB_setNotificationLevel
    Set amount of notification to be displayed on the console.
    default initial value : 0 = no console notification.
    Note : not thread-safe (use a global constant)
*/
void DiB_setNotificationLevel(unsigned l);


#endif
