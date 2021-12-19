/*--

This file is a part of libsais, a library for linear time
suffix array and burrows wheeler transform construction.

   Copyright (c) 2021 Ilya Grebnov <ilya.grebnov@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

Please see the file LICENSE for full copyright information.

--*/

#ifndef LIBSAIS_H
#define LIBSAIS_H 1

#ifdef __cplusplus
extern "C" {
#endif

    #include "../common/mem.h"

    /**
    * Creates the libsais context that allows reusing allocated memory with each libsais operation. 
    * In multi-threaded environments, use one context per thread for parallel executions.
    * @return the libsais context, NULL otherwise.
    */
    void * libsais_create_ctx(void);

#if defined(_OPENMP)
    /**
    * Creates the libsais context that allows reusing allocated memory with each parallel libsais operation using OpenMP. 
    * In multi-threaded environments, use one context per thread for parallel executions.
    * @param threads The number of OpenMP threads to use (can be 0 for OpenMP default).
    * @return the libsais context, NULL otherwise.
    */
    void * libsais_create_ctx_omp(S32 threads);
#endif

    /**
    * Destroys the libsass context and free previusly allocated memory.
    * @param ctx The libsais context (can be NULL).
    */
    void libsais_free_ctx(void * ctx);

    /**
    * Constructs the suffix array of a given string.
    * @param T [0..n-1] The input string.
    * @param SA [0..n-1+fs] The output array of suffixes.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of SA array (can be 0).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais(const U8 * T, S32 * SA, S32 n, S32 fs, S32 * freq);

    /**
    * Constructs the suffix array of a given string using libsais context.
    * @param ctx The libsais context.
    * @param T [0..n-1] The input string.
    * @param SA [0..n-1+fs] The output array of suffixes.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of SA array (can be 0).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_ctx(const void * ctx, const U8 * T, S32 * SA, S32 n, S32 fs, S32 * freq);

#if defined(_OPENMP)
    /**
    * Constructs the suffix array of a given string in parallel using OpenMP.
    * @param T [0..n-1] The input string.
    * @param SA [0..n-1+fs] The output array of suffixes.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of SA array (can be 0).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @param threads The number of OpenMP threads to use (can be 0 for OpenMP default).
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_omp(const U8 * T, S32 * SA, S32 n, S32 fs, S32 * freq, S32 threads);
#endif

    /**
    * Constructs the burrows-wheeler transformed string of a given string.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n-1+fs] The temporary array.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of A array (can be 0).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @return The primary index if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_bwt(const U8 * T, U8 * U, S32 * A, S32 n, S32 fs, S32 * freq);

    /**
    * Constructs the burrows-wheeler transformed string of a given string with auxiliary indexes.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n-1+fs] The temporary array.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of A array (can be 0).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @param r The sampling rate for auxiliary indexes (must be power of 2).
    * @param I [0..(n-1)/r] The output auxiliary indexes.
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_bwt_aux(const U8 * T, U8 * U, S32 * A, S32 n, S32 fs, S32 * freq, S32 r, S32 * I);

    /**
    * Constructs the burrows-wheeler transformed string of a given string using libsais context.
    * @param ctx The libsais context.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n-1+fs] The temporary array.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of A array (can be 0).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @return The primary index if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_bwt_ctx(const void * ctx, const U8 * T, U8 * U, S32 * A, S32 n, S32 fs, S32 * freq);

    /**
    * Constructs the burrows-wheeler transformed string of a given string with auxiliary indexes using libsais context.
    * @param ctx The libsais context.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n-1+fs] The temporary array.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of A array (can be 0).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @param r The sampling rate for auxiliary indexes (must be power of 2).
    * @param I [0..(n-1)/r] The output auxiliary indexes.
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_bwt_aux_ctx(const void * ctx, const U8 * T, U8 * U, S32 * A, S32 n, S32 fs, S32 * freq, S32 r, S32 * I);

#if defined(_OPENMP)
    /**
    * Constructs the burrows-wheeler transformed string of a given string in parallel using OpenMP.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n-1+fs] The temporary array.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of A array (can be 0).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @param threads The number of OpenMP threads to use (can be 0 for OpenMP default).
    * @return The primary index if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_bwt_omp(const U8 * T, U8 * U, S32 * A, S32 n, S32 fs, S32 * freq, S32 threads);

    /**
    * Constructs the burrows-wheeler transformed string of a given string with auxiliary indexes in parallel using OpenMP.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n-1+fs] The temporary array.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of A array (can be 0).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @param r The sampling rate for auxiliary indexes (must be power of 2).
    * @param I [0..(n-1)/r] The output auxiliary indexes.
    * @param threads The number of OpenMP threads to use (can be 0 for OpenMP default).
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_bwt_aux_omp(const U8 * T, U8 * U, S32 * A, S32 n, S32 fs, S32 * freq, S32 r, S32 * I, S32 threads);
#endif

    /**
    * Creates the libsais reverse BWT context that allows reusing allocated memory with each libsais_unbwt_* operation. 
    * In multi-threaded environments, use one context per thread for parallel executions.
    * @return the libsais context, NULL otherwise.
    */
    void * libsais_unbwt_create_ctx(void);

#if defined(_OPENMP)
    /**
    * Creates the libsais reverse BWT context that allows reusing allocated memory with each parallel libsais_unbwt_* operation using OpenMP. 
    * In multi-threaded environments, use one context per thread for parallel executions.
    * @param threads The number of OpenMP threads to use (can be 0 for OpenMP default).
    * @return the libsais context, NULL otherwise.
    */
    void * libsais_unbwt_create_ctx_omp(S32 threads);
#endif

    /**
    * Destroys the libsass reverse BWT context and free previusly allocated memory.
    * @param ctx The libsais context (can be NULL).
    */
    void libsais_unbwt_free_ctx(void * ctx);

    /**
    * Constructs the original string from a given burrows-wheeler transformed string with primary index.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n] The temporary array (NOTE, temporary array must be n + 1 size).
    * @param n The length of the given string.
    * @param freq [0..255] The input symbol frequency table (can be NULL).
    * @param i The primary index.
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_unbwt(const U8 * T, U8 * U, S32 * A, S32 n, const S32 * freq, S32 i);

    /**
    * Constructs the original string from a given burrows-wheeler transformed string with primary index using libsais reverse BWT context.
    * @param ctx The libsais reverse BWT context.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n] The temporary array (NOTE, temporary array must be n + 1 size).
    * @param n The length of the given string.
    * @param freq [0..255] The input symbol frequency table (can be NULL).
    * @param i The primary index.
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_unbwt_ctx(const void * ctx, const U8 * T, U8 * U, S32 * A, S32 n, const S32 * freq, S32 i);

    /**
    * Constructs the original string from a given burrows-wheeler transformed string with auxiliary indexes.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n] The temporary array (NOTE, temporary array must be n + 1 size).
    * @param n The length of the given string.
    * @param freq [0..255] The input symbol frequency table (can be NULL).
    * @param r The sampling rate for auxiliary indexes (must be power of 2).
    * @param I [0..(n-1)/r] The input auxiliary indexes.
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_unbwt_aux(const U8 * T, U8 * U, S32 * A, S32 n, const S32 * freq, S32 r, const S32 * I);

    /**
    * Constructs the original string from a given burrows-wheeler transformed string with auxiliary indexes using libsais reverse BWT context.
    * @param ctx The libsais reverse BWT context.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n] The temporary array (NOTE, temporary array must be n + 1 size).
    * @param n The length of the given string.
    * @param freq [0..255] The input symbol frequency table (can be NULL).
    * @param r The sampling rate for auxiliary indexes (must be power of 2).
    * @param I [0..(n-1)/r] The input auxiliary indexes.
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_unbwt_aux_ctx(const void * ctx, const U8 * T, U8 * U, S32 * A, S32 n, const S32 * freq, S32 r, const S32 * I);

#if defined(_OPENMP)
    /**
    * Constructs the original string from a given burrows-wheeler transformed string with primary index in parallel using OpenMP.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n] The temporary array (NOTE, temporary array must be n + 1 size).
    * @param n The length of the given string.
    * @param freq [0..255] The input symbol frequency table (can be NULL).
    * @param i The primary index.
    * @param threads The number of OpenMP threads to use (can be 0 for OpenMP default).
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_unbwt_omp(const U8 * T, U8 * U, S32 * A, S32 n, const S32 * freq, S32 i, S32 threads);

    /**
    * Constructs the original string from a given burrows-wheeler transformed string with auxiliary indexes in parallel using OpenMP.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n] The temporary array (NOTE, temporary array must be n + 1 size).
    * @param n The length of the given string.
    * @param freq [0..255] The input symbol frequency table (can be NULL).
    * @param r The sampling rate for auxiliary indexes (must be power of 2).
    * @param I [0..(n-1)/r] The input auxiliary indexes.
    * @param threads The number of OpenMP threads to use (can be 0 for OpenMP default).
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_unbwt_aux_omp(const U8 * T, U8 * U, S32 * A, S32 n, const S32 * freq, S32 r, const S32 * I, S32 threads);
#endif

#ifdef __cplusplus
}
#endif

#endif
