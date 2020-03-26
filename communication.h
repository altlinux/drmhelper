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

typedef enum {
	CMD_STATUS_DONE = 0,
	CMD_STATUS_FAILED,
} cmd_status_t;

struct cmd {
	cmd_t  type;
	size_t datalen;
};

struct cmd_resp {
	cmd_status_t status;
	size_t msglen;
};

#include <sys/types.h>
#include <sys/socket.h>

ssize_t sendmsg_retry(int sockfd, const struct msghdr *msg, int flags) __attribute__((nonnull(2)));
ssize_t recvmsg_retry(int sockfd, struct msghdr *msg, int flags) __attribute__((nonnull(2)));

// #include <unistd.h>
//
// int srv_listen(const char *, const char *)  __attribute__((nonnull(1, 2)));
// int srv_connect(const char *, const char *) __attribute__((nonnull(1, 2)));
//
// int get_peercred(int, pid_t *, uid_t *, gid_t *);
int set_recv_timeout(int fd, int secs);

int fds_send(int conn, int *fds, size_t fds_len);
int fds_recv(int conn, int *fds, size_t n_fds);

int xsendmsg(int conn, void *data, size_t len) __attribute__((nonnull(2)));
int xrecvmsg(int conn, void *data, size_t len) __attribute__((nonnull(2)));

#endif
