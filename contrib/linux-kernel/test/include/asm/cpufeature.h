/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */
#ifndef ASM_CPUFEATURE_H
#define ASM_CPUFEATURE_H

#define X86_FEATURE_BMI2 0
#define cpu_feature_enabled(feature) ((void)(feature), 0)

#endif
