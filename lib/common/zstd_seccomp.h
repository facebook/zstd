/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */
#ifndef _ZSTD_SECCOMP_H_
#define _ZSTD_SECCOMP_H_

/**
 * These functions use OS APIs, if present/enabled, to voluntarily give up the
 * ability to make certain system calls. Specifically, we will whitelist only
 * the syscalls we expect to make, and disable the rest. This is a defensive
 * measure, so that if someone were to figure out a remote code execution bug
 * in Zstd, they would be limited to a small set of syscalls, hopefully without
 * really any of the interesting ones (exec, clone, accept, connect, etc.).
 */

/**
 * Restrict this thread's capabilities down to the syscalls used by the zstdmt
 * worker threads.
 *
 * Return: 0 if failed. 1 if succeeded.
 */
int ZSTD_disable_syscalls_for_worker_thread(void);

#endif /* _ZSTD_SECCOMP_H_ */
