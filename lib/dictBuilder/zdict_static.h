/*
    dictBuilder header file
    for static linking only
    Copyright (C) Yann Collet 2016

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
       - Zstd source repository : https://www.zstd.net
*/

/* This library is EXPERIMENTAL, below API is not yet stable */

#ifndef DICTBUILDER_STATIC_H_002
#define DICTBUILDER_STATIC_H_002

#if defined (__cplusplus)
extern "C" {
#endif

/*-*************************************
*  Dependencies
***************************************/
#include "zdict.h"


/*-*************************************
*  Public type
***************************************/
typedef struct {
    unsigned selectivityLevel;   /* 0 means default; larger => bigger selection => larger dictionary */
    unsigned compressionLevel;   /* 0 means default; target a specific zstd compression level */
    unsigned notificationLevel;  /* Write to stderr; 0 = none (default); 1 = errors; 2 = progression; 3 = details; 4 = debug; */
    unsigned dictID;             /* 0 means auto mode (32-bits random value); other : force dictID value */
    unsigned reserved[2];        /* space for future parameters */
} ZDICT_params_t;


/*-*************************************
*  Public functions
***************************************/
/*! ZDICT_trainFromBuffer_advanced() :
    Same as ZDICT_trainFromBuffer() with control over more parameters.
    `parameters` is optional and can be provided with values set to 0 to mean "default".
    @return : size of dictionary stored into `dictBuffer` (<= `dictBufferSize`)
              or an error code, which can be tested by ZDICT_isError().
    note : ZDICT_trainFromBuffer_advanced() will send notifications into stderr if instructed to, using ZDICT_setNotificationLevel()
*/
size_t ZDICT_trainFromBuffer_advanced(void* dictBuffer, size_t dictBufferCapacity,
                             const void* samplesBuffer, const size_t* samplesSizes, unsigned nbSamples,
                             ZDICT_params_t parameters);


#if defined (__cplusplus)
}
#endif

#endif  /* DICTBUILDER_STATIC_H_002 */
