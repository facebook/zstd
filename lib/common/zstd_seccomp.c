/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "zstd_seccomp.h"

#include "debug.h" /* DEBUGLOG */

#ifdef ZSTD_FORCE_USE_SECCOMP
#  ifndef ZSTD_USE_SECCOMP
#    define ZSTD_USE_SECCOMP 1
#  endif
#endif

#ifdef ZSTD_TRY_USE_SECCOMP
/* Impl only works on linux >= 4.14, and only on x86 & amd64. */
#  if (defined(__linux__) || defined(__linux)) && (defined(__i386__) || defined(__x86_64__))
#    include <linux/version.h>
#    if LINUX_VERSION_CODE > KERNEL_VERSION(4,14,0)
#      ifndef ZSTD_USE_SECCOMP
#        define ZSTD_USE_SECCOMP 1
#      endif
#    endif
#  endif
#endif

#ifndef ZSTD_USE_SECCOMP
#  define ZSTD_USE_SECCOMP 0
#endif

#if ZSTD_USE_SECCOMP
#include <stddef.h> /* offsetof */
#include <unistd.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#define syscall_nr (offsetof(struct seccomp_data, nr))
#define arch_nr (offsetof(struct seccomp_data, arch))

#if defined(__i386__)
#  define REG_SYSCALL  REG_EAX
#  define ARCH_NR  AUDIT_ARCH_I386
#elif defined(__x86_64__)
#  define REG_SYSCALL  REG_RAX
#  define ARCH_NR  AUDIT_ARCH_X86_64
#else
#  error "Platform does not support seccomp filter."
#endif

#define VALIDATE_ARCHITECTURE \
  BPF_STMT(BPF_LD+BPF_W+BPF_ABS, arch_nr), \
  BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ARCH_NR, 1, 0), \
  BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL)

#define EXAMINE_SYSCALL \
  BPF_STMT(BPF_LD+BPF_W+BPF_ABS, syscall_nr)

#define ALLOW_SYSCALL(name) \
  BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_##name, 0, 1), \
  BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)

#define ALLOW \
  BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)

#define KILL_PROCESS \
  BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL)

#endif

int ZSTD_disable_syscalls_for_worker_thread(void) {
#if ZSTD_USE_SECCOMP
    struct sock_filter filter[] = {
        VALIDATE_ARCHITECTURE,
        EXAMINE_SYSCALL,
        ALLOW_SYSCALL(futex),
        ALLOW_SYSCALL(brk),
        ALLOW_SYSCALL(mmap),
        ALLOW_SYSCALL(munmap),
        ALLOW_SYSCALL(mprotect),
        ALLOW_SYSCALL(madvise),
        ALLOW_SYSCALL(read),
        ALLOW_SYSCALL(write),
        ALLOW_SYSCALL(exit),
        KILL_PROCESS
    };
    struct sock_fprog prog = {
       (unsigned short)(sizeof(filter)/sizeof(filter[0])),
       filter
    };
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
        DEBUGLOG(3, "prctl() failed: %m\n");
        return 0;
    }
    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_LOG, &prog)) {
        DEBUGLOG(3, "seccomp() failed: %m\n");
        return 0;
    }
#endif
    return 1;
}
