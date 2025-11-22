/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <stdio.h>
#define ZSTD_STATIC_LINKING_ONLY
#include "huf.h"

int main(void) {
    int i;
    unsigned hist[256];

    /* Equal frequency for all 256 symbols - forces raw write path */
    for (i = 0; i < 256; i++) {
        hist[i] = 100;
    }

    /* Build Huffman CTable */
    HUF_CElt CTable[HUF_CTABLE_SIZE_ST(255)];
    unsigned wksp[HUF_CTABLE_WORKSPACE_SIZE_U32];

    size_t result = HUF_buildCTable_wksp(CTable, hist, 255, 0, wksp, sizeof(wksp));
    if (HUF_isError(result)) {
        return 1;
    }

    /* Get table parameters */
    HUF_CTableHeader header = HUF_readCTableHeader(CTable);

    /* Write CTable - this triggers the bug */
    unsigned char dst[130];
    result = HUF_writeCTable_wksp(dst, sizeof(dst), CTable,
                                  header.maxSymbolValue, header.tableLog,
                                  wksp, sizeof(wksp));

    if (HUF_isError(result)) {
        printf("FAIL\n");
        return 1;
    }

    printf("OK\n");
    return 0;
}
