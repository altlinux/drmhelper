// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#ifndef _communication_h_
#define _communication_h_

#include <stdint.h>

typedef enum {
	CMD_NONE = 0,

	CMD_DRM_OPEN,
	CMD_DRM_SET_MASTER,
	CMD_DRM_DROP_MASTER,
} cmd_t;

struct cmd {
	cmd_t  type;
	size_t datalen;
};

#include <sys/types.h>
#include <sys/socket.h>

ssize_t sendmsg_retry(int sockfd, const struct msghdr *msg, int flags) __attribute__((nonnull(2)));
ssize_t recvmsg_retry(int sockfd, struct msghdr *msg, int flags) __attribute__((nonnull(2)));

int set_recv_timeout(int fd, int secs);

int fds_send(int conn, int *fds, size_t fds_len);
int fds_recv(int conn, int *fds, size_t n_fds);

int xsendmsg(int conn, void *data, size_t len) __attribute__((nonnull(2)));
int xrecvmsg(int conn, void *data, size_t len) __attribute__((nonnull(2)));

#include <stdlib.h>
#include <stdio.h>

#define err(format, arg...) \
	do { \
		fprintf(stderr, "%s(%d): " format, __FILE__, __LINE__, ##arg); \
		fprintf(stderr, "\n"); \
	} while(0)

#define fatal(format, arg...) \
	do { \
		err(format, ##arg); \
		exit(EXIT_FAILURE); \
	} while (0)

#ifdef USE_SECCOMP
int seccomp_filter(void);
#endif

#endif
