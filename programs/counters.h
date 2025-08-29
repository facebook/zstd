/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/****************************************************************************
 * Performance counters
 *
 ****************************************************************************/
#ifndef BENCH_ZSTD_COUNTERS
#define BENCH_ZSTD_COUNTERS
/* FIXME(cavalcanti): only include this for Linux (!Android)@x86 */
#include <inttypes.h>
#include <x86intrin.h>
#include <stdio.h>
#define _GNU_SOURCE
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <inttypes.h>
#include <sys/types.h>

typedef struct {
    struct perf_event_attr events;
    int fd;
    long long cycles;
} BMK_linuxPerfCounters_t;

static int BMK_countersOpen(BMK_linuxPerfCounters_t* counters)
{
    pid_t pid = 0;
    int cpu = -1;
    int group_fd = -1;
    unsigned long flags = 0;

    counters->fd = syscall(__NR_perf_event_open, &counters->events, pid, cpu,
                           group_fd, flags);

    if (counters->fd != -1) return 0;

    return -1;
}

static int BMK_countersInit(BMK_linuxPerfCounters_t* counters)
{
    memset(counters, 0, sizeof(struct perf_event_attr));
    counters->events.type = PERF_TYPE_HARDWARE;
    counters->events.size = sizeof(struct perf_event_attr);
    /* TODO(cavalcanti): Add more performance counters:
     * PERF_COUNT_HW_INSTRUCTIONS, PERF_COUNT_HW_BRANCH_MISSES,
     * PERF_COUNT_HW_CACHE_REFERENCES, PERF_COUNT_HW_CACHE_MISSES.
     */
    counters->events.config = PERF_COUNT_HW_CPU_CYCLES;
    counters->events.disabled = 1;
    counters->events.exclude_kernel = 1;
    counters->events.exclude_hv = 1;

    counters->cycles = 0;

    return BMK_countersOpen(counters);
}

static int BMK_eventStart(BMK_linuxPerfCounters_t* counters)
{
    int res = 0;
    if (counters->fd != -1) {
        res = ioctl(counters->fd, PERF_EVENT_IOC_RESET, 0);
        if (res != -1) res = ioctl(counters->fd, PERF_EVENT_IOC_ENABLE, 0);
    }

    return res;
}

static int BMK_eventStop(BMK_linuxPerfCounters_t* counters)
{
    long long count = 0;
    ioctl(counters->fd, PERF_EVENT_IOC_DISABLE, 0);
    if (read(counters->fd, &count, sizeof(long long)) == -1) return -1;
    counters->cycles += count;
}

static int BMK_countersClose(BMK_linuxPerfCounters_t* counters)
{
    close(counters->fd);
}

#endif
