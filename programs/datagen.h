/*
    datagen.h - compressible data generator header
    Copyright (C) Yann Collet 2012-2015

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
   - ZSTD source repository : https://github.com/Cyan4973/zstd
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
*/


#include <stddef.h>   /* size_t */

void RDG_genStdout(unsigned long long size, double matchProba, double litProba, unsigned seed);
void RDG_genBuffer(void* buffer, size_t size, double matchProba, double litProba, unsigned seed);
/* RDG_genBuffer
   Generate 'size' bytes of compressible data into 'buffer'.
   Compressibility can be controlled using 'matchProba', which is floating point value between 0 and 1.
   'LitProba' is optional, it affect variability of individual bytes. If litProba==0.0, default value will be used.
   Generated data pattern can be modified using different 'seed'.
   For a triplet (matchProba, litProba, seed), the function always generate the same content.

   RDG_genStdout
   Same as RDG_genBuffer, but generates data into stdout
*/
