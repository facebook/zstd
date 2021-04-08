/*
 * Copyright (c) Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef LEVEL
# error LEVEL(x) must be defined
#endif
#ifndef FAST_LEVEL
# error FAST_LEVEL(x) must be defined
#endif
#ifndef ROW_LEVEL
# error ROW_LEVEL(x, y) must be defined
#endif

/**
 * The levels are chosen to trigger every strategy in every source size,
 * as well as some fast levels and the default level.
 * If you change the compression levels, you should probably update these.
 */

FAST_LEVEL(5)

FAST_LEVEL(3)

FAST_LEVEL(1)
LEVEL(0)
LEVEL(1)

LEVEL(3)
LEVEL(4)
/* ROW_LEVEL triggers the row hash (force enabled and disabled) with different
 * dictionary strategies, and 16/32 row entries based on the level/searchLog.
 * 1 == disabled, 2 == enabled.
 */
ROW_LEVEL(5, 1)
ROW_LEVEL(5, 2)
LEVEL(5)
LEVEL(6)
ROW_LEVEL(7, 1)
ROW_LEVEL(7, 2)
LEVEL(7)

LEVEL(9)

ROW_LEVEL(12, 1)
ROW_LEVEL(12, 2)
LEVEL(13)

LEVEL(16)

LEVEL(19)
