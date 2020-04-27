// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/prctl.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"

#include "libdrmhelper.h"

#define DRMHELPER_DEFAULT_PATHNAME LIBEXECDIR "/drmhelper"

static char *drmhelper_pathname = (char *) DRMHELPER_DEFAULT_PATHNAME;

static void do_child(int conn)
{
	if (prctl(PR_SET_PDEATHSIG, SIGKILL) < 0)
		fatal("libdrmhelper: prctl(PR_SET_PDEATHSIG): %m");

	if (dup2(conn, STDIN_FILENO) != STDIN_FILENO)
		fatal("libdrmhelper: dup2: %m");

	char *const argv[] = { drmhelper_pathname, NULL };
	execv(argv[0], argv);

	fatal("libdrmhelper: execv: %s: %m", drmhelper_pathname);
}

static int execute_helper(pid_t *pid, int *sock)
{
	int sv[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
		err("libdrmhelper: socketpair: %m");
		return -1;
	}

	pid_t child = fork();

	if (child < 0) {
		err("libdrmhelper: fork: %m");
		return -1;
	}

	if (!child) {
		close(sv[0]);
		do_child(sv[1]);
	}

	set_recv_timeout(sv[0], 3);

	close(sv[1]);

	*pid = child;
	*sock = sv[0];

	return 0;
}

int drmhelper_have_permissions(void)
{
	return !access(drmhelper_pathname, X_OK);
}

int drmhelper_open(const char *path)
{
	int ret = -1;
	int conn = -1;
	pid_t helper = -1;

	int status;
	struct sigaction saved_action, action;

	action.sa_handler = SIG_DFL;
	action.sa_flags = SA_RESTART;

	sigemptyset(&action.sa_mask);

	if (sigaction(SIGCHLD, &action, &saved_action) < 0) {
		err("libdrmhelper: sigaction: %m");
		return -1;
	}

	if (execute_helper(&helper, &conn) < 0)
		goto end;

	size_t len = strlen(path) + 1;

	struct cmd hdr = {
		.type = CMD_DRM_OPEN,
		.datalen = len,
	};

	if (xsendmsg(conn, &hdr, sizeof(hdr)) < 0)
		goto end;

	if (xsendmsg(conn, (char *) path, len) < 0)
		goto end;

	int drmfd;

	if (fds_recv(conn, &drmfd, 1) < 0)
		goto end;

	ret = drmfd;
end:
	if (conn >= 0)
		close(conn);

	if (helper > 0 && TEMP_FAILURE_RETRY(waitpid(helper, &status, 0)) < 0)
		err("libdrmhelper: waitpid: %m");

	(void) sigaction(SIGCHLD, &saved_action, 0);

	return ret;
}

static int set_master(int drmfd, cmd_t cmd)
{
	int ret = -1;
	int conn = -1;
	pid_t helper = -1;

	int status;
	struct sigaction saved_action, action;

	action.sa_handler = SIG_DFL;
	action.sa_flags = SA_RESTART;

	sigemptyset(&action.sa_mask);

	if (sigaction(SIGCHLD, &action, &saved_action) < 0) {
		err("libdrmhelper: sigaction: %m");
		return -1;
	}

	if (execute_helper(&helper, &conn) < 0)
		goto end;

	struct cmd hdr = {
		.type = cmd,
		.datalen = sizeof(drmfd),
	};

	if (xsendmsg(conn, &hdr, sizeof(hdr)) < 0)
		goto end;

	if (fds_send(conn, &drmfd, 1) < 0)
		goto end;

	xrecvmsg(conn, &ret, sizeof(ret));
end:
	if (conn >= 0)
		close(conn);

	if (helper > 0 && TEMP_FAILURE_RETRY(waitpid(helper, &status, 0)) < 0)
		err("libdrmhelper: waitpid: %m");

	(void) sigaction(SIGCHLD, &saved_action, 0);

	return ret;
}

int drmhelper_set_master(int drmfd)
{
	return set_master(drmfd, CMD_DRM_SET_MASTER);
}

int drmhelper_drop_master(int drmfd)
{
	return set_master(drmfd, CMD_DRM_DROP_MASTER);
}
