/*
    dictBuilder header file
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

#ifndef DICTBUILDER_H_001
#define DICTBUILDER_H_001

#if defined (__cplusplus)
extern "C" {
#endif

/*-*************************************
*  Public functions
***************************************/
/*! ZDICT_trainFromBuffer() :
    Train a dictionary from a memory buffer `samplesBuffer`,
    where `nbSamples` samples have been stored concatenated.
    Each sample size is provided into an orderly table `samplesSizes`.
    Resulting dictionary will be saved into `dictBuffer`.
    @return : size of dictionary stored into `dictBuffer` (<= `dictBufferCapacity`)
              or an error code, which can be tested by ZDICT_isError().
*/
size_t ZDICT_trainFromBuffer(void* dictBuffer, size_t dictBufferCapacity,
                             const void* samplesBuffer, const size_t* samplesSizes, unsigned nbSamples);


/*-*************************************
*  Helper functions
***************************************/
unsigned ZDICT_isError(size_t errorCode);
const char* ZDICT_getErrorName(size_t errorCode);


#if defined (__cplusplus)
}
#endif

#endif
