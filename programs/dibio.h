/*
    dibio.h - I/O API for dictionary builder
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
    - zstd homepage : http://www.zstd.net/
*/

/* This library is designed for a single-threaded console application.
*  It exit() and printf() into stderr when it encounters an error condition. */

#ifndef DIBIO_H_003
#define DIBIO_H_003


/*-*************************************
*  Dependencies
***************************************/
#include "dictBuilder_static.h"   /* ZDICT_params_t */


/*-*************************************
*  Public functions
***************************************/
/*! DiB_trainFromFiles() :
    Train a dictionary from a set of files provided by `fileNamesTable`.
    Resulting dictionary is written into file `dictFileName`.
    `parameters` is optional and can be provided with values set to 0, meaning "default".
    @return : 0 == ok. Any other : error.
*/
int DiB_trainFromFiles(const char* dictFileName, unsigned maxDictSize,
                       const char** fileNamesTable, unsigned nbFiles,
                       ZDICT_params_t parameters);


/*-*************************************
*  Helper functions
***************************************/
/*! DiB_setNotificationLevel
    Set amount of notification to be displayed on the console.
    default initial value : 0 = no console notification.
    Note : not thread-safe (use a global constant)
*/
void DiB_setNotificationLevel(unsigned l);


#endif
