/*
 * lfs.h for libdivsufsort
 * Copyright (c) 2003-2008 Yuta Mori All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _LFS_H
#define _LFS_H 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef __STRICT_ANSI__
# define LFS_OFF_T off_t
# define LFS_FOPEN fopen
# define LFS_FTELL ftello
# define LFS_FSEEK fseeko
# define LFS_PRId  PRIdMAX
#else
# define LFS_OFF_T long
# define LFS_FOPEN fopen
# define LFS_FTELL ftell
# define LFS_FSEEK fseek
# define LFS_PRId "ld"
#endif
#ifndef PRIdOFF_T
# define PRIdOFF_T LFS_PRId
#endif


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* _LFS_H */
