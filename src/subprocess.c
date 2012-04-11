/*	--*- c -*--
 * Copyright (C) 2012 Enrico Scholz <enrico.scholz@informatik.tu-chemnitz.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "subprocess.h"

#include <errno.h>
#include <stdio.h>

#include "util.h"

bool subprocess_init(struct subprocess *proc, bool is_interactive)
{
	bool		rc = false;
	size_t		num_p = 0;

	proc->is_init = false;
	proc->is_interactive = is_interactive;
	proc->is_spawned = false;

	proc->pid = -1;
	proc->pipe_ctl[0] = -1;
	proc->pipe_ctl[1] = -1;

	for (num_p = 0; num_p < ARRAY_SIZE(proc->pipe_std); ++num_p) {
		if (pipe(proc->pipe_std[num_p]) < 0) {
			perror("pipe(<std>)");
			goto out;
		}
	}

	if (xpipe(proc->pipe_ctl) < 0) {
		perror("pipe(<ctl>)");
		goto out;
	}

	if (set_cloexec(proc->pipe_ctl[0], true) < 0) {
		perror("fnctl(<ctlpipe[0]>, F_SETFD, FD_CLOEXEC");
		goto out;
	}

	if (set_cloexec(proc->pipe_ctl[1], true) < 0) {
		perror("fnctl(<ctlpipe[1]>, F_SETFD, FD_CLOEXEC");
		goto out;
	}

	proc->is_init = true;
	rc = true;

out:
	if (!rc) {
		xclose(proc->pipe_ctl[1]);	
		xclose(proc->pipe_ctl[0]);

		while (num_p > 0) {
			--num_p;
			xclose(proc->pipe_std[num_p][1]);
			xclose(proc->pipe_std[num_p][0]);
		}
	}

	return rc;
}

void subprocess_destroy(struct subprocess *proc)
{
	size_t		i;

	if (!proc || !proc->is_init)
		return;

	xclose(proc->pipe_ctl[1]);	
	xclose(proc->pipe_ctl[0]);

	for (i = ARRAY_SIZE(proc->pipe_std); i > 0; --i) {
		xclose(proc->pipe_std[i][1]);
		xclose(proc->pipe_std[i][0]);
	}

	if (proc->pid == -1)
		;			/* noop */
	else if (waitpid(proc->pid, NULL, 0) != proc->pid)
}

static void subprocess_child_exit(struct subprocess *proc, int retval,
				  char const *code, char const *details)
{
	if (details == NULL)
		details = strerror(errno);

	write(proc->pipe_ctl[1], code, strlen(code));
	write(proc->pipe_ctl[1], details, strlen(details));
	_exit(retval);
}

static void subprocess_child_run(struct subprocess *proc,
				 int argc, char *argv[])
{
	int		stdin_fd;

	if (proc->is_interactive)
		stdin_fd = proc->pipe_std[0][0];
	else {
		stdin_fd = open("/dev/null", O_RDONLY);
		if (stdin_fd < 0)
			subprocess_child_exit(proc, 1,
					      "E:open(/dev/null):", NULL);

		close(proc->pipe_std[0][0]);
	}

	if (dup2(stdin_fd, 0) < 0 ||
	    dup2(proc->pipe_std[1][1], 1) < 0 ||
	    dup2(proc->pipe_std[2][1], 1) < 0)
		subprocess_child_exit(proc, 1, "E:dup2:", NULL);
		
	close(proc->pipe_ctl[0]);
	close(proc->pipe_std[0][1]);
	close(proc->pipe_std[1][0]);
	close(proc->pipe_std[2][0]);

	if (stdin_fd > 0)
		close(stdin_fd);

	if (proc->pipe_std[1][1] > 0)
		close(proc->pipe_std[1][1]);

	if (proc->pipe_std[2][1] > 0)
		close(proc->pipe_std[2][1]);

	execvp(argv[0], &argv[0]);
	subprocess_child_exit(proc, 1, "E:execvp:", NULL);
}

static void subprocess_child_terminate(struct subprocess *proc)
{
	sigset_t		mask;
	sigset_t		old_mask;
	bool			have_oldmask = false;
	int			sfd = -1;
	struct signalfd_siginfo	fdsi;

	assert(proc->pid != -1);
	assert(proc->is_spawned);

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	if (sigprocmask(SIG_BLOCK, &mask, &old_mask) == -1) {
		perror("sigprocmask(<SIG_BLOCK>, <SIGCHLD>)");
		goto out;
	}
	have_oldmask = true;

	sfd = signalfd(-1, &mask, 0);
	if (sfd < 0) {
		perror("signalfd()");
		goto out;
	}

	if (wait4(proc->pid, NULL, WNOHANG, &proc->rusage) == proc->pid)
		proc->pid = -1;
	else if (kill(proc->pid, SIGTERM) < 0)
		perror("kill(<chld>, SIGTERM)");
	else {
		fd_set			fds;
		struct timeval		tv = {
			/* \todo: allow configuration of timeout */
			.tv_sec = 2,
			.tv_usec = 0,
		};

		FD_ZERO(&fds);
		FD_SET(sfd, &fds);

		/* \todo: this works only for one child; when process forks
		 * multiple children, this must be looped and it must be
		 * checked for the timeout manually */
		if (select(sfd + 1, &fds, NULL, NULL, &tv) == 1)
			;		/* noop */
		else if (kill(proc->pid, SIGKILL) < 0)
			perror("kill(<chld>, SIGKILL)");

		if (wait4(proc->pid, NULL, 0, &proc->rusage) == proc->pid)
			proc->pid = -1;
	}

out:
	xclose(sfd);
	if (have_oldmask)
		sigprocmask(SIG_SET, &old_mask, NULL);

	return;
}

static bool subprocess_main_start(struct subprocess *proc)
{
	bool		rc;
	char		buf[128];
	ssize_t		l;

	close(proc->pipe_ctl[1]);
	proc->ctl_pipe[1] = -1;

	close(proc->pipe_std[0][0]);
	proc->pipe_std[0][0] = -1;

	close(proc->pipe_std[1][1]);
	proc->pipe_std[1][1] = -1;

	close(proc->pipe_std[2][1]);
	proc->pipe_std[2][1] = -1;

	l = read(proc->pipe_ctl[0], buf, sizeof buf);
	if (l > 0) {
		fprintf(stderr, "internal error: %.*s", l, buf);
		rc = false;
	} else if (l < 0) {
		perror("read(<ctl_pipe>)");
		rc = false;
	} else
		rc = true;

	return rc;
}

bool subprocess_spawn(struct subprocess *proc, int argc, char *argv[])
{
	bool		rc = false;

	assert(proc->is_init);
	assert(!proc->is_spawned);

	proc->pid = fork();
	proc->is_spawned = proc->pid >= 0;

	if (proc->pid < 0) {
		goto out;
	else if (proc->pid == 0)
		subprocess_child_run(proc, argc, argv);
	else if (!subprocess_main_start(proc))
		subprocess_child_terminate(proc);
	else
		rc = true;
	
	return rc;
}

