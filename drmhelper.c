// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#include <linux/major.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <drm/drm.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "common.h"

#ifndef DRM_MAJOR
#define DRM_MAJOR 226
#endif

static int conn;

static inline int _ioctl(int fd, unsigned long request, void *arg)
{
	int ret;
	do {
		ret = ioctl(fd, request, arg);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
	return ret;
}

static int drm_set_master(int fd)
{
	return _ioctl(fd, DRM_IOCTL_SET_MASTER, NULL);
}

static int drm_drop_master(int fd)
{
	return _ioctl(fd, DRM_IOCTL_DROP_MASTER, NULL);
}

static int command_open(struct cmd *hdr)
{
	int ret = -1;
	int fd = -1;
	char path[PATH_MAX] = "";

	if (hdr->datalen >= sizeof(path))
		goto end;

	if (xrecvmsg(conn, &path, hdr->datalen) < 0)
		goto end;

	if (setuid(0) < 0) {
		ret = -errno;
		err("setuid: %m");
		goto end;
	}

	if ((fd = open(path, O_RDWR|O_CLOEXEC|O_NOCTTY|O_NONBLOCK)) < 0) {
		ret = -errno;
		err("open: %s: %m", path);
		goto end;
	}

	struct stat st;

	if (fstat(fd, &st) < 0) {
		ret = -errno;
		err("stat: %s: %m", path);
		goto end;
	}

	if ((st.st_mode & S_IFMT) != S_IFCHR) {
		ret = -ENOTSUP;
		err("unable to open not character device");
		goto end;
	}

	uint32_t maj = major(st.st_rdev);

	if (maj != INPUT_MAJOR && maj != DRM_MAJOR) {
		ret = -ENOTSUP;
		goto end;
	}

	if (maj == DRM_MAJOR && drm_set_master(fd) < 0) {
		ret = -errno;
		err("ioctl(DRM_IOCTL_SET_MASTER): %s: %m", path);
		goto end;
	}

	ret = fd;
end:
	fds_send(conn, &ret, 1);

	return ( ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int command_set_master(struct cmd *hdr)
{
	int fd = -1;
	int ret = -1;

	if (hdr->datalen != sizeof(int)) {
		ret = -EINVAL;
		goto end;
	}

	if (fds_recv(conn, &fd, 1) < 0)
		goto end;

	if (setuid(0) < 0) {
		ret = -errno;
		err("setuid: %m");
		goto end;
	}

	if (drm_set_master(fd) < 0) {
		ret = -errno;
		err("ioctl(DRM_IOCTL_SET_MASTER): %m");
		goto end;
	}

	ret = 0;
end:
	xsendmsg(conn, &ret, sizeof(ret));

	return ( ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int command_drop_master(struct cmd *hdr)
{
	int fd = -1;
	int ret = -1;

	if (hdr->datalen != sizeof(int)) {
		ret = -EINVAL;
		goto end;
	}

	if (fds_recv(conn, &fd, 1) < 0)
		goto end;

	if (setuid(0) < 0) {
		ret = -errno;
		err("setuid: %m");
		goto end;
	}

	if (drm_drop_master(fd) < 0) {
		ret = -errno;
		err("ioctl(DRM_IOCTL_DROP_MASTER): %m");
		goto end;
	}

	ret = 0;
end:
	xsendmsg(conn, &ret, sizeof(ret));

	return ( ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(void)
{
	conn = STDIN_FILENO;

	struct stat st;
	if (fstat(conn, &st) < 0)
		fatal("stat(stdin): %m");

	if ((st.st_mode & S_IFMT) != S_IFSOCK)
		fatal("stdin is not a socket");

	set_recv_timeout(conn, 3);

	int ret = -1;
	struct cmd hdr = { 0 };

	if (xrecvmsg(conn, &hdr, sizeof(hdr)) < 0)
		goto error;

	switch (hdr.type) {
		case CMD_DRM_OPEN:
			return command_open(&hdr);

		case CMD_DRM_SET_MASTER:
			return command_set_master(&hdr);

		case CMD_DRM_DROP_MASTER:
			return command_drop_master(&hdr);
		default:
			break;
	}

	err("unknown command: %d", hdr.type);
error:
	xsendmsg(conn, &ret, sizeof(ret));
	return EXIT_FAILURE;
}
