// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "logging.h"
#include "communication.h"

int
set_recv_timeout(int fd, int secs)
{
	struct timeval tv = { .tv_sec = secs };

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		err("setsockopt(SO_RCVTIMEO): %m");
		return -1;
	}
	return 0;
}

	ssize_t
sendmsg_retry(int sockfd, const struct msghdr *msg, int flags)
{
	return TEMP_FAILURE_RETRY(sendmsg(sockfd, msg, flags));
}

ssize_t
recvmsg_retry(int sockfd, struct msghdr *msg, int flags)
{
	return TEMP_FAILURE_RETRY(recvmsg(sockfd, msg, flags));
}

int
fds_send(int conn, int *fds, size_t fds_len)
{
	ssize_t rc;
	size_t len = sizeof(int) * fds_len;

	union {
		char buf[CMSG_SPACE(len)];
		struct cmsghdr align;
	} u;

	/*
	 * We must send at least 1 byte of real data in
	 * order to send ancillary data
	*/
	char dummy;

	struct iovec iov = { 0 };

	iov.iov_base = &dummy;
	iov.iov_len  = sizeof(dummy);

	struct msghdr msg = { 0 };

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type  = SCM_RIGHTS;
	cmsg->cmsg_len   = CMSG_LEN(len);

	memcpy(CMSG_DATA(cmsg), fds, len);

	if ((rc = sendmsg_retry(conn, &msg, 0)) != sizeof(dummy)) {
		if (rc < 0)
			err("sendmsg: %m");
		else if (rc)
			err("sendmsg: expected size %lu, got %lu", sizeof(dummy), rc);
		else
			err("sendmsg: unexpected EOF");
		return -1;
	}

	return 0;
}

int
fds_recv(int conn, int *fds, size_t n_fds)
{
	char dummy;
	size_t datalen = sizeof(int) * n_fds;

	union {
		struct cmsghdr cmh;
		char   control[CMSG_SPACE(datalen)];
	} u;

	u.cmh.cmsg_len   = CMSG_LEN(datalen);
	u.cmh.cmsg_level = SOL_SOCKET;
	u.cmh.cmsg_type  = SCM_RIGHTS;

	struct iovec iov = { 0 };

	iov.iov_base = &dummy;
	iov.iov_len  = sizeof(dummy);

	struct msghdr msg = { 0 };

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = u.control;
	msg.msg_controllen = sizeof(u.control);

	ssize_t n;
	if ((n = recvmsg_retry(conn, &msg, 0)) != sizeof(dummy)) {
		if (n < 0)
			err("recvmsg: %m");
		else if (n)
			err("recvmsg: expected size %lu, got %lu", sizeof(dummy), n);
		else
			err("recvmsg: unexpected EOF");
		return -1;
	}

	if (!msg.msg_controllen) {
		err("ancillary data not specified");
		return -1;
	}

	for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
			continue;

		if (cmsg->cmsg_len - CMSG_LEN(0) != datalen) {
			err("expected fd payload");
			return -1;
		}

		memcpy(fds, CMSG_DATA(cmsg), datalen);
		break;
	}

	return 0;
}
int
xsendmsg(int conn, void *data, size_t len)
{
	struct iovec iov = { 0 };
	struct msghdr msg = { 0 };

	iov.iov_base = data;
	iov.iov_len  = len;

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	ssize_t n = sendmsg_retry(conn, &msg, 0);

	if (n != (ssize_t) len) {
		if (n < 0)
			err("recvmsg: %m");
		else if (n)
			err("recvmsg: expected size %lu, got %lu", len, n);
		else
			err("recvmsg: unexpected EOF");
		return -1;
	}

	return 0;
}

int
xrecvmsg(int conn, void *data, size_t len)
{
	struct iovec iov = { 0 };
	struct msghdr msg = { 0 };

	iov.iov_base = data;
	iov.iov_len  = len;

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	ssize_t n = recvmsg_retry(conn, &msg, MSG_WAITALL);

	if (n != (ssize_t) len) {
		if (n < 0)
			err("recvmsg: %m");
		else if (n)
			err("recvmsg: expected size %lu, got %lu", len, n);
		else
			err("recvmsg: unexpected EOF");
		return -1;
	}

	return 0;
}
