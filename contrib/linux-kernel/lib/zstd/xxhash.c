/*
*  xxHash - Fast Hash algorithm
*  Copyright (C) 2012-2016, Yann Collet
*
*  BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are
*  met:
*
*  * Redistributions of source code must retain the above copyright
*  notice, this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above
*  copyright notice, this list of conditions and the following disclaimer
*  in the documentation and/or other materials provided with the
*  distribution.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*  You can contact the author at :
*  - xxHash homepage: http://www.xxhash.com
*  - xxHash source repository : https://github.com/Cyan4973/xxHash
*/


/* *************************************
*  Tuning parameters
***************************************/
/*!XXH_ACCEPT_NULL_INPUT_POINTER :
 * If the input pointer is a null pointer, xxHash default behavior is to trigger a memory access error, since it is a bad pointer.
 * When this option is enabled, xxHash output for null input pointers will be the same as a null-length input.
 * By default, this option is disabled. To enable it, uncomment below define :
 */
/* #define XXH_ACCEPT_NULL_INPUT_POINTER 1 */

/*!XXH_FORCE_NATIVE_FORMAT :
 * By default, xxHash library provides endian-independant Hash values, based on little-endian convention.
 * Results are therefore identical for little-endian and big-endian CPU.
 * This comes at a performance cost for big-endian CPU, since some swapping is required to emulate little-endian format.
 * Should endian-independance be of no importance for your application, you may set the #define below to 1,
 * to improve speed for Big-endian CPU.
 * This option has no impact on Little_Endian CPU.
 */
#define XXH_FORCE_NATIVE_FORMAT 0

/*!XXH_FORCE_ALIGN_CHECK :
 * This is a minor performance trick, only useful with lots of very small keys.
 * It means : check for aligned/unaligned input.
 * The check costs one initial branch per hash; set to 0 when the input data
 * is guaranteed to be aligned.
 */
#define XXH_FORCE_ALIGN_CHECK 0


/* *************************************
*  Includes & Memory related functions
***************************************/
/* Modify the local functions below should you wish to use some other memory routines */
/* for memcpy() */
#include <linux/string.h>
static void* XXH_memcpy(void* dest, const void* src, size_t size) { return memcpy(dest,src,size); }

#include "xxhash.h"
#include "mem.h"


/* *************************************
*  Compiler Specific Options
***************************************/
#include <linux/compiler.h>
#define FORCE_INLINE static __always_inline


/* ****************************************
*  Compiler-specific Functions and Macros
******************************************/
#define XXH_rotl32(x,r) ((x << r) | (x >> (32 - r)))
#define XXH_rotl64(x,r) ((x << r) | (x >> (64 - r)))

/* *************************************
*  Architecture Macros
***************************************/
typedef enum { XXH_bigEndian=0, XXH_littleEndian=1 } XXH_endianess;

/* XXH_CPU_LITTLE_ENDIAN can be defined externally, for example on the compiler command line */
#ifndef XXH_CPU_LITTLE_ENDIAN
#   define XXH_CPU_LITTLE_ENDIAN   MEM_LITTLE_ENDIAN
#endif


/* ***************************
*  Memory reads
*****************************/
typedef enum { XXH_aligned, XXH_unaligned } XXH_alignment;

FORCE_INLINE U32 XXH_readLE32_align(const void* ptr, XXH_endianess endian, XXH_alignment align)
{
	(void)endian;
	(void)align;
	return MEM_readLE32(ptr);
}

FORCE_INLINE U32 XXH_readLE32(const void* ptr, XXH_endianess endian)
{
	return XXH_readLE32_align(ptr, endian, XXH_unaligned);
}

static U32 XXH_readBE32(const void* ptr)
{
	return MEM_readBE32(ptr);
}

FORCE_INLINE U64 XXH_readLE64_align(const void* ptr, XXH_endianess endian, XXH_alignment align)
{
	(void)endian;
	(void)align;
	return MEM_readLE64(ptr);
}

FORCE_INLINE U64 XXH_readLE64(const void* ptr, XXH_endianess endian)
{
	return XXH_readLE64_align(ptr, endian, XXH_unaligned);
}

static U64 XXH_readBE64(const void* ptr)
{
	return MEM_readBE64(ptr);
}


/* *************************************
*  Macros
***************************************/
#define XXH_STATIC_ASSERT(c)   { enum { XXH_static_assert = 1/(int)(!!(c)) }; }    /* use only *after* variable declarations */


/* *************************************
*  Constants
***************************************/
static const U32 PRIME32_1 = 2654435761U;
static const U32 PRIME32_2 = 2246822519U;
static const U32 PRIME32_3 = 3266489917U;
static const U32 PRIME32_4 =  668265263U;
static const U32 PRIME32_5 =  374761393U;

static const U64 PRIME64_1 = 11400714785074694791ULL;
static const U64 PRIME64_2 = 14029467366897019727ULL;
static const U64 PRIME64_3 =  1609587929392839161ULL;
static const U64 PRIME64_4 =  9650029242287828579ULL;
static const U64 PRIME64_5 =  2870177450012600261ULL;

XXH_PUBLIC_API unsigned XXH_versionNumber (void) { return XXH_VERSION_NUMBER; }


/* **************************
*  Utils
****************************/
XXH_PUBLIC_API void XXH32_copyState(XXH32_state_t* dstState, const XXH32_state_t* srcState)
{
	memcpy(dstState, srcState, sizeof(*dstState));
}

XXH_PUBLIC_API void XXH64_copyState(XXH64_state_t* dstState, const XXH64_state_t* srcState)
{
	memcpy(dstState, srcState, sizeof(*dstState));
}


/* ***************************
*  Simple Hash Functions
*****************************/

static U32 XXH32_round(U32 seed, U32 input)
{
	seed += input * PRIME32_2;
	seed  = XXH_rotl32(seed, 13);
	seed *= PRIME32_1;
	return seed;
}

FORCE_INLINE U32 XXH32_endian_align(const void* input, size_t len, U32 seed, XXH_endianess endian, XXH_alignment align)
{
	const BYTE* p = (const BYTE*)input;
	const BYTE* bEnd = p + len;
	U32 h32;
#define XXH_get32bits(p) XXH_readLE32_align(p, endian, align)

#ifdef XXH_ACCEPT_NULL_INPUT_POINTER
	if (p==NULL) {
		len=0;
		bEnd=p=(const BYTE*)(size_t)16;
	}
#endif

	if (len>=16) {
		const BYTE* const limit = bEnd - 16;
		U32 v1 = seed + PRIME32_1 + PRIME32_2;
		U32 v2 = seed + PRIME32_2;
		U32 v3 = seed + 0;
		U32 v4 = seed - PRIME32_1;

		do {
			v1 = XXH32_round(v1, XXH_get32bits(p)); p+=4;
			v2 = XXH32_round(v2, XXH_get32bits(p)); p+=4;
			v3 = XXH32_round(v3, XXH_get32bits(p)); p+=4;
			v4 = XXH32_round(v4, XXH_get32bits(p)); p+=4;
		} while (p<=limit);

		h32 = XXH_rotl32(v1, 1) + XXH_rotl32(v2, 7) + XXH_rotl32(v3, 12) + XXH_rotl32(v4, 18);
	} else {
		h32  = seed + PRIME32_5;
	}

	h32 += (U32) len;

	while (p+4<=bEnd) {
		h32 += XXH_get32bits(p) * PRIME32_3;
		h32  = XXH_rotl32(h32, 17) * PRIME32_4 ;
		p+=4;
	}

	while (p<bEnd) {
		h32 += (*p) * PRIME32_5;
		h32 = XXH_rotl32(h32, 11) * PRIME32_1 ;
		p++;
	}

	h32 ^= h32 >> 15;
	h32 *= PRIME32_2;
	h32 ^= h32 >> 13;
	h32 *= PRIME32_3;
	h32 ^= h32 >> 16;

	return h32;
}


XXH_PUBLIC_API unsigned int XXH32 (const void* input, size_t len, unsigned int seed)
{
#if 0
	/* Simple version, good for code maintenance, but unfortunately slow for small inputs */
	XXH32_CREATESTATE_STATIC(state);
	XXH32_reset(state, seed);
	XXH32_update(state, input, len);
	return XXH32_digest(state);
#else
	XXH_endianess endian_detected = (XXH_endianess)XXH_CPU_LITTLE_ENDIAN;

	if (XXH_FORCE_ALIGN_CHECK) {
		if ((((size_t)input) & 3) == 0) {   /* Input is 4-bytes aligned, leverage the speed benefit */
			if ((endian_detected==XXH_littleEndian) || XXH_FORCE_NATIVE_FORMAT)
				return XXH32_endian_align(input, len, seed, XXH_littleEndian, XXH_aligned);
			else
				return XXH32_endian_align(input, len, seed, XXH_bigEndian, XXH_aligned);
	}   }

	if ((endian_detected==XXH_littleEndian) || XXH_FORCE_NATIVE_FORMAT)
		return XXH32_endian_align(input, len, seed, XXH_littleEndian, XXH_unaligned);
	else
		return XXH32_endian_align(input, len, seed, XXH_bigEndian, XXH_unaligned);
#endif
}


static U64 XXH64_round(U64 acc, U64 input)
{
	acc += input * PRIME64_2;
	acc  = XXH_rotl64(acc, 31);
	acc *= PRIME64_1;
	return acc;
}

static U64 XXH64_mergeRound(U64 acc, U64 val)
{
	val  = XXH64_round(0, val);
	acc ^= val;
	acc  = acc * PRIME64_1 + PRIME64_4;
	return acc;
}

FORCE_INLINE U64 XXH64_endian_align(const void* input, size_t len, U64 seed, XXH_endianess endian, XXH_alignment align)
{
	const BYTE* p = (const BYTE*)input;
	const BYTE* const bEnd = p + len;
	U64 h64;
#define XXH_get64bits(p) XXH_readLE64_align(p, endian, align)

#ifdef XXH_ACCEPT_NULL_INPUT_POINTER
	if (p==NULL) {
		len=0;
		bEnd=p=(const BYTE*)(size_t)32;
	}
#endif

	if (len>=32) {
		const BYTE* const limit = bEnd - 32;
		U64 v1 = seed + PRIME64_1 + PRIME64_2;
		U64 v2 = seed + PRIME64_2;
		U64 v3 = seed + 0;
		U64 v4 = seed - PRIME64_1;

		do {
			v1 = XXH64_round(v1, XXH_get64bits(p)); p+=8;
			v2 = XXH64_round(v2, XXH_get64bits(p)); p+=8;
			v3 = XXH64_round(v3, XXH_get64bits(p)); p+=8;
			v4 = XXH64_round(v4, XXH_get64bits(p)); p+=8;
		} while (p<=limit);

		h64 = XXH_rotl64(v1, 1) + XXH_rotl64(v2, 7) + XXH_rotl64(v3, 12) + XXH_rotl64(v4, 18);
		h64 = XXH64_mergeRound(h64, v1);
		h64 = XXH64_mergeRound(h64, v2);
		h64 = XXH64_mergeRound(h64, v3);
		h64 = XXH64_mergeRound(h64, v4);

	} else {
		h64  = seed + PRIME64_5;
	}

	h64 += (U64) len;

	while (p+8<=bEnd) {
		U64 const k1 = XXH64_round(0, XXH_get64bits(p));
		h64 ^= k1;
		h64  = XXH_rotl64(h64,27) * PRIME64_1 + PRIME64_4;
		p+=8;
	}

	if (p+4<=bEnd) {
		h64 ^= (U64)(XXH_get32bits(p)) * PRIME64_1;
		h64 = XXH_rotl64(h64, 23) * PRIME64_2 + PRIME64_3;
		p+=4;
	}

	while (p<bEnd) {
		h64 ^= (*p) * PRIME64_5;
		h64 = XXH_rotl64(h64, 11) * PRIME64_1;
		p++;
	}

	h64 ^= h64 >> 33;
	h64 *= PRIME64_2;
	h64 ^= h64 >> 29;
	h64 *= PRIME64_3;
	h64 ^= h64 >> 32;

	return h64;
}


XXH_PUBLIC_API unsigned long long XXH64 (const void* input, size_t len, unsigned long long seed)
{
#if 0
	/* Simple version, good for code maintenance, but unfortunately slow for small inputs */
	XXH64_CREATESTATE_STATIC(state);
	XXH64_reset(state, seed);
	XXH64_update(state, input, len);
	return XXH64_digest(state);
#else
	XXH_endianess endian_detected = (XXH_endianess)XXH_CPU_LITTLE_ENDIAN;

	if (XXH_FORCE_ALIGN_CHECK) {
		if ((((size_t)input) & 7)==0) {  /* Input is aligned, let's leverage the speed advantage */
			if ((endian_detected==XXH_littleEndian) || XXH_FORCE_NATIVE_FORMAT)
				return XXH64_endian_align(input, len, seed, XXH_littleEndian, XXH_aligned);
			else
				return XXH64_endian_align(input, len, seed, XXH_bigEndian, XXH_aligned);
	}   }

	if ((endian_detected==XXH_littleEndian) || XXH_FORCE_NATIVE_FORMAT)
		return XXH64_endian_align(input, len, seed, XXH_littleEndian, XXH_unaligned);
	else
		return XXH64_endian_align(input, len, seed, XXH_bigEndian, XXH_unaligned);
#endif
}


/* **************************************************
*  Advanced Hash Functions
****************************************************/


/*** Hash feed ***/

XXH_PUBLIC_API XXH_errorcode XXH32_reset(XXH32_state_t* statePtr, unsigned int seed)
{
	XXH32_state_t state;   /* using a local state to memcpy() in order to avoid strict-aliasing warnings */
	memset(&state, 0, sizeof(state)-4);   /* do not write into reserved, for future removal */
	state.v1 = seed + PRIME32_1 + PRIME32_2;
	state.v2 = seed + PRIME32_2;
	state.v3 = seed + 0;
	state.v4 = seed - PRIME32_1;
	memcpy(statePtr, &state, sizeof(state));
	return XXH_OK;
}


XXH_PUBLIC_API XXH_errorcode XXH64_reset(XXH64_state_t* statePtr, unsigned long long seed)
{
	XXH64_state_t state;   /* using a local state to memcpy() in order to avoid strict-aliasing warnings */
	memset(&state, 0, sizeof(state)-8);   /* do not write into reserved, for future removal */
	state.v1 = seed + PRIME64_1 + PRIME64_2;
	state.v2 = seed + PRIME64_2;
	state.v3 = seed + 0;
	state.v4 = seed - PRIME64_1;
	memcpy(statePtr, &state, sizeof(state));
	return XXH_OK;
}


FORCE_INLINE XXH_errorcode XXH32_update_endian (XXH32_state_t* state, const void* input, size_t len, XXH_endianess endian)
{
	const BYTE* p = (const BYTE*)input;
	const BYTE* const bEnd = p + len;

#ifdef XXH_ACCEPT_NULL_INPUT_POINTER
	if (input==NULL) return XXH_ERROR;
#endif

	state->total_len_32 += (unsigned)len;
	state->large_len |= (len>=16) | (state->total_len_32>=16);

	if (state->memsize + len < 16)  {   /* fill in tmp buffer */
		XXH_memcpy((BYTE*)(state->mem32) + state->memsize, input, len);
		state->memsize += (unsigned)len;
		return XXH_OK;
	}

	if (state->memsize) {   /* some data left from previous update */
		XXH_memcpy((BYTE*)(state->mem32) + state->memsize, input, 16-state->memsize);
		{   const U32* p32 = state->mem32;
			state->v1 = XXH32_round(state->v1, XXH_readLE32(p32, endian)); p32++;
			state->v2 = XXH32_round(state->v2, XXH_readLE32(p32, endian)); p32++;
			state->v3 = XXH32_round(state->v3, XXH_readLE32(p32, endian)); p32++;
			state->v4 = XXH32_round(state->v4, XXH_readLE32(p32, endian)); p32++;
		}
		p += 16-state->memsize;
		state->memsize = 0;
	}

	if (p <= bEnd-16) {
		const BYTE* const limit = bEnd - 16;
		U32 v1 = state->v1;
		U32 v2 = state->v2;
		U32 v3 = state->v3;
		U32 v4 = state->v4;

		do {
			v1 = XXH32_round(v1, XXH_readLE32(p, endian)); p+=4;
			v2 = XXH32_round(v2, XXH_readLE32(p, endian)); p+=4;
			v3 = XXH32_round(v3, XXH_readLE32(p, endian)); p+=4;
			v4 = XXH32_round(v4, XXH_readLE32(p, endian)); p+=4;
		} while (p<=limit);

		state->v1 = v1;
		state->v2 = v2;
		state->v3 = v3;
		state->v4 = v4;
	}

	if (p < bEnd) {
		XXH_memcpy(state->mem32, p, (size_t)(bEnd-p));
		state->memsize = (unsigned)(bEnd-p);
	}

	return XXH_OK;
}

XXH_PUBLIC_API XXH_errorcode XXH32_update (XXH32_state_t* state_in, const void* input, size_t len)
{
	XXH_endianess endian_detected = (XXH_endianess)XXH_CPU_LITTLE_ENDIAN;

	if ((endian_detected==XXH_littleEndian) || XXH_FORCE_NATIVE_FORMAT)
		return XXH32_update_endian(state_in, input, len, XXH_littleEndian);
	else
		return XXH32_update_endian(state_in, input, len, XXH_bigEndian);
}



FORCE_INLINE U32 XXH32_digest_endian (const XXH32_state_t* state, XXH_endianess endian)
{
	const BYTE * p = (const BYTE*)state->mem32;
	const BYTE* const bEnd = (const BYTE*)(state->mem32) + state->memsize;
	U32 h32;

	if (state->large_len) {
		h32 = XXH_rotl32(state->v1, 1) + XXH_rotl32(state->v2, 7) + XXH_rotl32(state->v3, 12) + XXH_rotl32(state->v4, 18);
	} else {
		h32 = state->v3 /* == seed */ + PRIME32_5;
	}

	h32 += state->total_len_32;

	while (p+4<=bEnd) {
		h32 += XXH_readLE32(p, endian) * PRIME32_3;
		h32  = XXH_rotl32(h32, 17) * PRIME32_4;
		p+=4;
	}

	while (p<bEnd) {
		h32 += (*p) * PRIME32_5;
		h32  = XXH_rotl32(h32, 11) * PRIME32_1;
		p++;
	}

	h32 ^= h32 >> 15;
	h32 *= PRIME32_2;
	h32 ^= h32 >> 13;
	h32 *= PRIME32_3;
	h32 ^= h32 >> 16;

	return h32;
}


XXH_PUBLIC_API unsigned int XXH32_digest (const XXH32_state_t* state_in)
{
	XXH_endianess endian_detected = (XXH_endianess)XXH_CPU_LITTLE_ENDIAN;

	if ((endian_detected==XXH_littleEndian) || XXH_FORCE_NATIVE_FORMAT)
		return XXH32_digest_endian(state_in, XXH_littleEndian);
	else
		return XXH32_digest_endian(state_in, XXH_bigEndian);
}



/* **** XXH64 **** */

FORCE_INLINE XXH_errorcode XXH64_update_endian (XXH64_state_t* state, const void* input, size_t len, XXH_endianess endian)
{
	const BYTE* p = (const BYTE*)input;
	const BYTE* const bEnd = p + len;

#ifdef XXH_ACCEPT_NULL_INPUT_POINTER
	if (input==NULL) return XXH_ERROR;
#endif

	state->total_len += len;

	if (state->memsize + len < 32) {  /* fill in tmp buffer */
		XXH_memcpy(((BYTE*)state->mem64) + state->memsize, input, len);
		state->memsize += (U32)len;
		return XXH_OK;
	}

	if (state->memsize) {   /* tmp buffer is full */
		XXH_memcpy(((BYTE*)state->mem64) + state->memsize, input, 32-state->memsize);
		state->v1 = XXH64_round(state->v1, XXH_readLE64(state->mem64+0, endian));
		state->v2 = XXH64_round(state->v2, XXH_readLE64(state->mem64+1, endian));
		state->v3 = XXH64_round(state->v3, XXH_readLE64(state->mem64+2, endian));
		state->v4 = XXH64_round(state->v4, XXH_readLE64(state->mem64+3, endian));
		p += 32-state->memsize;
		state->memsize = 0;
	}

	if (p+32 <= bEnd) {
		const BYTE* const limit = bEnd - 32;
		U64 v1 = state->v1;
		U64 v2 = state->v2;
		U64 v3 = state->v3;
		U64 v4 = state->v4;

		do {
			v1 = XXH64_round(v1, XXH_readLE64(p, endian)); p+=8;
			v2 = XXH64_round(v2, XXH_readLE64(p, endian)); p+=8;
			v3 = XXH64_round(v3, XXH_readLE64(p, endian)); p+=8;
			v4 = XXH64_round(v4, XXH_readLE64(p, endian)); p+=8;
		} while (p<=limit);

		state->v1 = v1;
		state->v2 = v2;
		state->v3 = v3;
		state->v4 = v4;
	}

	if (p < bEnd) {
		XXH_memcpy(state->mem64, p, (size_t)(bEnd-p));
		state->memsize = (unsigned)(bEnd-p);
	}

	return XXH_OK;
}

XXH_PUBLIC_API XXH_errorcode XXH64_update (XXH64_state_t* state_in, const void* input, size_t len)
{
	XXH_endianess endian_detected = (XXH_endianess)XXH_CPU_LITTLE_ENDIAN;

	if ((endian_detected==XXH_littleEndian) || XXH_FORCE_NATIVE_FORMAT)
		return XXH64_update_endian(state_in, input, len, XXH_littleEndian);
	else
		return XXH64_update_endian(state_in, input, len, XXH_bigEndian);
}



FORCE_INLINE U64 XXH64_digest_endian (const XXH64_state_t* state, XXH_endianess endian)
{
	const BYTE * p = (const BYTE*)state->mem64;
	const BYTE* const bEnd = (const BYTE*)state->mem64 + state->memsize;
	U64 h64;

	if (state->total_len >= 32) {
		U64 const v1 = state->v1;
		U64 const v2 = state->v2;
		U64 const v3 = state->v3;
		U64 const v4 = state->v4;

		h64 = XXH_rotl64(v1, 1) + XXH_rotl64(v2, 7) + XXH_rotl64(v3, 12) + XXH_rotl64(v4, 18);
		h64 = XXH64_mergeRound(h64, v1);
		h64 = XXH64_mergeRound(h64, v2);
		h64 = XXH64_mergeRound(h64, v3);
		h64 = XXH64_mergeRound(h64, v4);
	} else {
		h64  = state->v3 + PRIME64_5;
	}

	h64 += (U64) state->total_len;

	while (p+8<=bEnd) {
		U64 const k1 = XXH64_round(0, XXH_readLE64(p, endian));
		h64 ^= k1;
		h64  = XXH_rotl64(h64,27) * PRIME64_1 + PRIME64_4;
		p+=8;
	}

	if (p+4<=bEnd) {
		h64 ^= (U64)(XXH_readLE32(p, endian)) * PRIME64_1;
		h64  = XXH_rotl64(h64, 23) * PRIME64_2 + PRIME64_3;
		p+=4;
	}

	while (p<bEnd) {
		h64 ^= (*p) * PRIME64_5;
		h64  = XXH_rotl64(h64, 11) * PRIME64_1;
		p++;
	}

	h64 ^= h64 >> 33;
	h64 *= PRIME64_2;
	h64 ^= h64 >> 29;
	h64 *= PRIME64_3;
	h64 ^= h64 >> 32;

	return h64;
}


XXH_PUBLIC_API unsigned long long XXH64_digest (const XXH64_state_t* state_in)
{
	XXH_endianess endian_detected = (XXH_endianess)XXH_CPU_LITTLE_ENDIAN;

	if ((endian_detected==XXH_littleEndian) || XXH_FORCE_NATIVE_FORMAT)
		return XXH64_digest_endian(state_in, XXH_littleEndian);
	else
		return XXH64_digest_endian(state_in, XXH_bigEndian);
}


/* **************************
*  Canonical representation
****************************/

/*! Default XXH result types are basic unsigned 32 and 64 bits.
*   The canonical representation follows human-readable write convention, aka big-endian (large digits first).
*   These functions allow transformation of hash result into and from its canonical format.
*   This way, hash values can be written into a file or buffer, and remain comparable across different systems and programs.
*/

XXH_PUBLIC_API void XXH32_canonicalFromHash(XXH32_canonical_t* dst, XXH32_hash_t hash)
{
	XXH_STATIC_ASSERT(sizeof(XXH32_canonical_t) == sizeof(XXH32_hash_t));
	MEM_writeBE32(dst, hash);
}

XXH_PUBLIC_API void XXH64_canonicalFromHash(XXH64_canonical_t* dst, XXH64_hash_t hash)
{
	XXH_STATIC_ASSERT(sizeof(XXH64_canonical_t) == sizeof(XXH64_hash_t));
	MEM_writeBE64(dst, hash);
}

XXH_PUBLIC_API XXH32_hash_t XXH32_hashFromCanonical(const XXH32_canonical_t* src)
{
	return XXH_readBE32(src);
}

XXH_PUBLIC_API XXH64_hash_t XXH64_hashFromCanonical(const XXH64_canonical_t* src)
{
	return XXH_readBE64(src);
}
