// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>

#include <sys/prctl.h>

#include <stddef.h>
#include <seccomp.h>
#include <errno.h>

#include "common.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) \
	(sizeof(x) / sizeof((x)[0]))
#endif

#if defined(__x86_64__)
#define ARCH_NR AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
#define ARCH_NR AUDIT_ARCH_AARCH64
#elif defined(__arm__)
#define ARCH_NR AUDIT_ARCH_ARM
#elif defined(__mips64__)
#define ARCH_NR AUDIT_ARCH_MIPS64
#elif defined(__mips__)
#define ARCH_NR AUDIT_ARCH_MIPS
#elif defined(__i386__)
#define ARCH_NR AUDIT_ARCH_I386
#else
#warning "Platform does not support seccomp filter"
#define ARCH_NR 0
#endif

#define syscall_nr (offsetof(struct seccomp_data, nr))
#define arch_nr    (offsetof(struct seccomp_data, arch))

#define KILL_PROCESS \
	BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL)

#define CHECK_ARCHITECTURE \
	BPF_STMT(BPF_LD | BPF_W | BPF_ABS, arch_nr), \
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ARCH_NR, 1, 0), \
	KILL_PROCESS

#define LOAD_SYSCALL_NR \
	BPF_STMT(BPF_LD | BPF_W | BPF_ABS, syscall_nr)

#define ALLOW_SYSCALL(name) \
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_##name, 0, 1), \
	BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)

static struct sock_filter filter[] = {
	CHECK_ARCHITECTURE,
	LOAD_SYSCALL_NR,
	ALLOW_SYSCALL(rt_sigreturn),
#ifdef __NR_sigreturn
	ALLOW_SYSCALL(sigreturn),
#endif
	ALLOW_SYSCALL(exit_group),
	ALLOW_SYSCALL(exit),
	ALLOW_SYSCALL(write),
	ALLOW_SYSCALL(openat),
	ALLOW_SYSCALL(fstat),
#ifdef __NR_lstat // __aarch64__
	ALLOW_SYSCALL(lstat),
#endif
	ALLOW_SYSCALL(setuid),
	ALLOW_SYSCALL(setsockopt),
	ALLOW_SYSCALL(sendmsg),
	ALLOW_SYSCALL(recvmsg),
	ALLOW_SYSCALL(ioctl),
	KILL_PROCESS,
};

int seccomp_filter(void)
{
#if ARCH_NR == 0
	err("Platform does not support seccomp filter");
	return 0;
#endif
	struct sock_fprog sec_filter = {
		.len    = ARRAY_SIZE(filter),
		.filter = filter,
	};

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		err("prctl(NO_NEW_PRIVS): %m");
		goto error;
	}

	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &sec_filter)) {
		err("prctl(SECCOMP): %m");
		goto error;
	}

	return 0;
error:
	if (errno == EINVAL)
		err("SECCOMP_FILTER is not available");
	return -1;
}
