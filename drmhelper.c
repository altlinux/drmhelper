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

#include "logging.h"
#include "communication.h"

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

static void command_open(struct cmd *hdr)
{
	int ret = -1;
	int fd = -1;
	char path[PATH_MAX] = "";
	cmd_status_t status = CMD_STATUS_FAILED;

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
	status = CMD_STATUS_DONE;
end:
	fds_send(conn, &ret, 1);

	if (fd >= 0)
		close(fd);

	xsendmsg(conn, &status, sizeof(status));
}

static void command_set_master(struct cmd *hdr)
{
	int fd = -1;
	cmd_status_t status = CMD_STATUS_FAILED;

	if (hdr->datalen != sizeof(int))
		goto end;

	if (fds_recv(conn, &fd, 1) < 0)
		goto end;

	if (setuid(0) < 0) {
		err("setuid: %m");
		goto end;
	}

	if (drm_set_master(fd) < 0) {
		err("ioctl(DRM_IOCTL_SET_MASTER): %m");
		goto end;
	}

	status = CMD_STATUS_DONE;
end:
	if (fd >= 0)
		close(fd);
	xsendmsg(conn, &status, sizeof(status));
}

static void command_drop_master(struct cmd *hdr)
{
	int fd = -1;
	cmd_status_t status = CMD_STATUS_FAILED;

	if (hdr->datalen != sizeof(int))
		goto end;

	if (fds_recv(conn, &fd, 1) < 0)
		goto end;

	if (setuid(0) < 0) {
		err("setuid: %m");
		goto end;
	}

	if (drm_drop_master(fd) < 0) {
		err("ioctl(DRM_IOCTL_DROP_MASTER): %m");
		goto end;
	}

	status = CMD_STATUS_DONE;
end:
	if (fd >= 0)
		close(fd);
	xsendmsg(conn, &status, sizeof(status));
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

	cmd_status_t status = CMD_STATUS_FAILED;
	struct cmd hdr = { 0 };

	if (xrecvmsg(conn, &hdr, sizeof(hdr)) < 0) {
		xsendmsg(conn, &status, sizeof(status));
		return EXIT_FAILURE;
	}

	switch (hdr.type) {
		case CMD_DRM_OPEN:
			command_open(&hdr);
			break;
		case CMD_DRM_SET_MASTER:
			command_set_master(&hdr);
			break;
		case CMD_DRM_DROP_MASTER:
			command_drop_master(&hdr);
			break;
		default:
			err("unknown command: %d", hdr.type);
			xsendmsg(conn, &status, sizeof(status));
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
