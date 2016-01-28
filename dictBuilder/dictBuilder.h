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
*  It abruptly exits (exit() function) when it encounters an error condition. */

/*-*************************************
*  Version
***************************************/
#define DiB_VERSION_MAJOR    0    /* for breaking interface changes  */
#define DiB_VERSION_MINOR    0    /* for new (non-breaking) interface capabilities */
#define DiB_VERSION_RELEASE  1    /* for tweaks, bug-fixes, or development */
#define DiB_VERSION_NUMBER  (DiB_VERSION_MAJOR *100*100 + DiB_VERSION_MINOR *100 + DiB_VERSION_RELEASE)
unsigned DiB_versionNumber (void);


/*-*************************************
*  Main functions
***************************************/
/*! DiB_trainDictionary
    Train a dictionary from a set of files provided by @fileNamesTable
    Resulting dictionary is written in file @dictFileName
    @result : 0 if fine
*/
int DiB_trainDictionary(const char* dictFileName, unsigned maxDictSize, unsigned selectivityLevel,
                        const char** fileNamesTable, unsigned nbFiles);


/*! DiB_setNotificationLevel
    Set amount of notification to be displayed on the console.
    0 = no console notification (default).
    Note : not thread-safe (use a global constant)
*/
void DiB_setNotificationLevel(unsigned l);
