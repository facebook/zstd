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

#ifndef LIBSAIS_INTERNAL_H
#define LIBSAIS_INTERNAL_H 1

#ifdef __cplusplus
extern "C" {
#endif

    #include "../common/mem.h"

    /**
    * Internal method to construct suffix array of an integer array. 
    * Note, during suffix array construction input array will be modified and restored at the end if no error occurred.
    * @param T [0..n-1] The input integer array.
    * @param SA [0..n-1+fs] The output array of suffixes.
    * @param n The length of the integer array.
    * @param k The alphabet size of the input integer array.
    * @param fs Extra space available at the end of SA array (can be 0).
    * @param threads The number of OpenMP threads to use (can be 0 for OpenMP default).
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    S32 libsais_main_32s_internal(S32 * T, S32 * SA, S32 n, S32 k, S32 fs, S32 threads);
#ifdef __cplusplus
}
#endif

#endif
