/* ##########################################################################
# namespaceTest
# ensure xxhash namespace emulation is properly triggered
# Copyright (C) Yann Collet 2016
#
# GPL v2 License
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# You can contact the author at :
#  - zstd homepage : http://www.zstd.net/
# ########################################################################*/


#include <stddef.h>  /* size_t */
#include <string.h>  /* strlen */

/* symbol definition */
extern unsigned XXH32(const void* src, size_t srcSize, unsigned seed);

int main(int argc, const char** argv)
{
    const char* exename = argv[0];
    unsigned result = XXH32(exename, strlen(exename), argc);
    return !result;
}
