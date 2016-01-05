/*
   xxHash - Extremely Fast Hash algorithm
   Header File
   Copyright (C) 2012-2016, Yann Collet.

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
   - xxHash source repository : https://github.com/Cyan4973/xxHash
*/

/* Notice extracted from xxHash homepage :

xxHash is an extremely fast Hash algorithm, running at RAM speed limits.
It also successfully passes all tests from the SMHasher suite.

Comparison (single thread, Windows Seven 32 bits, using SMHasher on a Core 2 Duo @3GHz)

Name            Speed       Q.Score   Author
xxHash          5.4 GB/s     10
CrapWow         3.2 GB/s      2       Andrew
MumurHash 3a    2.7 GB/s     10       Austin Appleby
SpookyHash      2.0 GB/s     10       Bob Jenkins
SBox            1.4 GB/s      9       Bret Mulvey
Lookup3         1.2 GB/s      9       Bob Jenkins
SuperFastHash   1.2 GB/s      1       Paul Hsieh
CityHash64      1.05 GB/s    10       Pike & Alakuijala
FNV             0.55 GB/s     5       Fowler, Noll, Vo
CRC32           0.43 GB/s     9
MD5-32          0.33 GB/s    10       Ronald L. Rivest
SHA1-32         0.28 GB/s    10

Q.Score is a measure of quality of the hash function.
It depends on successfully passing SMHasher test set.
10 is a perfect score.

A 64-bits version, named XXH64, is available since r35.
It offers much better speed, but for 64-bits applications only.
Name     Speed on 64 bits    Speed on 32 bits
XXH64       13.8 GB/s            1.9 GB/s
XXH32        6.8 GB/s            6.0 GB/s
*/

#pragma once

#if defined (__cplusplus)
extern "C" {
#endif


/* ****************************
*  Definitions
******************************/
#include <stddef.h>   /* size_t */
typedef enum { XXH_OK=0, XXH_ERROR } XXH_errorcode;


/* ****************************
*  API modifier
******************************/
/*!XXH_PRIVATE_API
*  Transforms all publics symbols within `xxhash.c` into private ones.
*  Methodology :
*  instead of : #include "xxhash.h"
*  do :
*     #define XXH_PRIVATE_API
*     #include "xxhash.c"   // note the .c , instead of .h
*  also : don't compile and link xxhash.c separately
*/
#ifdef XXH_PRIVATE_API
#  if defined(__GNUC__)
#    define XXH_PUBLIC_API static __attribute__((unused))
#  elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#    define XXH_PUBLIC_API static inline
#  elif defined(_MSC_VER)
#    define XXH_PUBLIC_API static __inline
#  else
#    define XXH_PUBLIC_API static   /* this version may generate warnings for unused static functions; disable the relevant warning */
#  endif
#else
#  define XXH_PUBLIC_API   /* do nothing */
#endif

/*!XXH_NAMESPACE, aka Namespace Emulation :

If you want to include _and expose_ xxHash functions from within your own library,
but also want to avoid symbol collisions with another library which also includes xxHash,

you can use XXH_NAMESPACE, to automatically prefix any public symbol from `xxhash.c`
with the value of XXH_NAMESPACE (so avoid to keep it NULL and avoid numeric values).

Note that no change is required within the calling program as long as it also includes `xxhash.h` :
regular symbol name will be automatically translated by this header.
*/
#ifdef XXH_NAMESPACE
#  define XXH_CAT(A,B) A##B
#  define XXH_NAME2(A,B) XXH_CAT(A,B)
#  define XXH32 XXH_NAME2(XXH_NAMESPACE, XXH32)
#  define XXH64 XXH_NAME2(XXH_NAMESPACE, XXH64)
#  define XXH_versionNumber XXH_NAME2(XXH_NAMESPACE, XXH_versionNumber)
#  define XXH32_createState XXH_NAME2(XXH_NAMESPACE, XXH32_createState)
#  define XXH64_createState XXH_NAME2(XXH_NAMESPACE, XXH64_createState)
#  define XXH32_freeState XXH_NAME2(XXH_NAMESPACE, XXH32_freeState)
#  define XXH64_freeState XXH_NAME2(XXH_NAMESPACE, XXH64_freeState)
#  define XXH32_reset XXH_NAME2(XXH_NAMESPACE, XXH32_reset)
#  define XXH64_reset XXH_NAME2(XXH_NAMESPACE, XXH64_reset)
#  define XXH32_update XXH_NAME2(XXH_NAMESPACE, XXH32_update)
#  define XXH64_update XXH_NAME2(XXH_NAMESPACE, XXH64_update)
#  define XXH32_digest XXH_NAME2(XXH_NAMESPACE, XXH32_digest)
#  define XXH64_digest XXH_NAME2(XXH_NAMESPACE, XXH64_digest)
#endif


/* *************************************
*  Version
***************************************/
#define XXH_VERSION_MAJOR    0    /* for breaking interface changes  */
#define XXH_VERSION_MINOR    5    /* for new (non-breaking) interface capabilities */
#define XXH_VERSION_RELEASE  0    /* for tweaks, bug-fixes, or development */
#define XXH_VERSION_NUMBER  (XXH_VERSION_MAJOR *100*100 + XXH_VERSION_MINOR *100 + XXH_VERSION_RELEASE)
XXH_PUBLIC_API unsigned XXH_versionNumber (void);


/* ****************************
*  Simple Hash Functions
******************************/

XXH_PUBLIC_API unsigned int       XXH32 (const void* input, size_t length, unsigned int seed);
XXH_PUBLIC_API unsigned long long XXH64 (const void* input, size_t length, unsigned long long seed);

/*!
XXH32() :
    Calculate the 32-bits hash of sequence "length" bytes stored at memory address "input".
    The memory between input & input+length must be valid (allocated and read-accessible).
    "seed" can be used to alter the result predictably.
    Speed on Core 2 Duo @ 3 GHz (single thread, SMHasher benchmark) : 5.4 GB/s
XXH64() :
    Calculate the 64-bits hash of sequence of length "len" stored at memory address "input".
    "seed" can be used to alter the result predictably.
    This function runs faster on 64-bits systems, but slower on 32-bits systems (see benchmark).
*/


/* ****************************
*  Advanced Hash Functions
******************************/
typedef struct XXH32_state_s XXH32_state_t;   /* incomplete */
typedef struct XXH64_state_s XXH64_state_t;   /* incomplete */


/*!Static allocation
   For static linking only, do not use in the context of DLL ! */
typedef struct { long long ll[ 6]; } XXH32_stateBody_t;
typedef struct { long long ll[11]; } XXH64_stateBody_t;

#define XXH32_CREATESTATE_STATIC(name) XXH32_stateBody_t name##xxhbody; void* name##xxhvoid = &(name##xxhbody); XXH32_state_t* name = (XXH32_state_t*)(name##xxhvoid)   /* no final ; */
#define XXH64_CREATESTATE_STATIC(name) XXH64_stateBody_t name##xxhbody; void* name##xxhvoid = &(name##xxhbody); XXH64_state_t* name = (XXH64_state_t*)(name##xxhvoid)   /* no final ; */


/*!Dynamic allocation
   To be preferred in the context of DLL */

XXH_PUBLIC_API XXH32_state_t* XXH32_createState(void);
XXH_PUBLIC_API XXH_errorcode  XXH32_freeState(XXH32_state_t* statePtr);

XXH_PUBLIC_API XXH64_state_t* XXH64_createState(void);
XXH_PUBLIC_API XXH_errorcode  XXH64_freeState(XXH64_state_t* statePtr);


/* hash streaming */

XXH_PUBLIC_API XXH_errorcode XXH32_reset  (XXH32_state_t* statePtr, unsigned int seed);
XXH_PUBLIC_API XXH_errorcode XXH32_update (XXH32_state_t* statePtr, const void* input, size_t length);
XXH_PUBLIC_API unsigned int  XXH32_digest (const XXH32_state_t* statePtr);

XXH_PUBLIC_API XXH_errorcode      XXH64_reset  (XXH64_state_t* statePtr, unsigned long long seed);
XXH_PUBLIC_API XXH_errorcode      XXH64_update (XXH64_state_t* statePtr, const void* input, size_t length);
XXH_PUBLIC_API unsigned long long XXH64_digest (const XXH64_state_t* statePtr);

/*!
These functions calculate the xxHash of an input provided in multiple segments,
as opposed to provided as a single block.

XXH state space must first be allocated, using either static or dynamic method provided above.

Start a new hash by initializing state with a seed, using XXHnn_reset().

Then, feed the hash state by calling XXHnn_update() as many times as necessary.
Obviously, input must be valid, meaning allocated and read accessible.
The function returns an error code, with 0 meaning OK, and any other value meaning there is an error.

Finally, a hash value can be produced anytime, by using XXHnn_digest().
This function returns the nn-bits hash.
It's nonetheless possible to continue inserting input into the hash state
and later on generate some new hashes, by calling again XXHnn_digest().

When you are done, don't forget to free XXH state space if it was allocated dynamically.
*/


#if defined (__cplusplus)
}
#endif
