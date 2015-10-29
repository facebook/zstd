/*
  fileio.h - file i/o handler
  Copyright (C) Yann Collet 2013-2015

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
#pragma once

#if defined (__cplusplus)
extern "C" {
#endif


/* *************************************
*  Special i/o constants
**************************************/
#define nullString "null"
#define stdinmark "-"
#define stdoutmark "-"
#ifdef _WIN32
#  define nulmark "nul"
#else
#  define nulmark "/dev/null"
#endif


/* *************************************
*  Parameters
***************************************/
void FIO_overwriteMode(void);
void FIO_setNotificationLevel(unsigned level);


/* *************************************
*  Stream/File functions
***************************************/
unsigned long long FIO_compressFilename (const char* outfilename, const char* infilename, int compressionLevel);
unsigned long long FIO_decompressFilename (const char* outfilename, const char* infilename);
/**
FIO_compressFilename :
    @result : size of compressed file

FIO_decompressFilename :
    @result : size of regenerated file
*/



#if defined (__cplusplus)
}
#endif
