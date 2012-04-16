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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/wait.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

#include "util.h"

bool subprocess_init(struct subprocess *proc, bool is_interactive)
{
	bool		rc = false;
	size_t		num_p = 0;

	proc->is_init = false;
	proc->is_interactive = is_interactive;
	proc->is_spawned = false;

	proc->pid = -1;
	proc->pipe_ctl.rd = -1;
	proc->pipe_ctl.wr = -1;
	proc->old_chld_mask = 0;

	for (num_p = 0; num_p < ARRAY_SIZE(proc->pipe_std); ++num_p) {
		if (pipe_create(&proc->pipe_std[num_p]) < 0) {
			perror("pipe(<std>)");
			goto out;
		}
	}

	if (pipe_create(&proc->pipe_ctl) < 0) {
		perror("pipe(<ctl>)");
		goto out;
	}

	if (set_cloexec(proc->pipe_ctl.rd, true) < 0) {
		perror("fnctl(<ctlpipe[0]>, F_SETFD, FD_CLOEXEC");
		goto out;
	}

	if (set_cloexec(proc->pipe_ctl.wr, true) < 0) {
		perror("fnctl(<ctlpipe[1]>, F_SETFD, FD_CLOEXEC");
		goto out;
	}

	proc->is_init = true;
	rc = true;

out:
	if (!rc) {
		pipe_close(&proc->pipe_ctl);

		while (num_p > 0) {
			--num_p;
			pipe_close(&proc->pipe_std[num_p]);
		}
	}

	return rc;
}

void subprocess_destroy(struct subprocess *proc)
{
	size_t		i;

	if (!proc || !proc->is_init)
		return;

	pipe_close(&proc->pipe_ctl);

	for (i = ARRAY_SIZE(proc->pipe_std); i > 0; --i)
		pipe_close(&proc->pipe_std[i-1]);

	if (proc->old_chld_mask) {
		sigset_t	mask;
		sigemptyset(&mask);
		sigaddset(&mask, SIG_BLOCK);

		if (proc->old_chld_mask < 0)
			sigprocmask(SIG_UNBLOCK, &mask, NULL);
	}

	if (proc->pid == -1)
		;			/* noop */
	else if (waitpid(proc->pid, NULL, 0) != proc->pid)
		perror("waitpid()");

	proc->pid = -1;
}

static void subprocess_child_exit(struct subprocess *proc, int retval,
				  char const *code, char const *details)
{
	if (details == NULL)
		details = strerror(errno);

	write_all(proc->pipe_ctl.wr, code, strlen(code));
	write_all(proc->pipe_ctl.wr, details, strlen(details));
	_exit(retval);
}

static void subprocess_child_run(struct subprocess *proc,
				 int argc, char *argv[],
				 subprocess_child_cleanup_fn *cleanup_fn,
				 void *priv)
{
	int		stdin_fd;

	if (proc->is_interactive)
		stdin_fd = proc->pipe_std[0].rd;
	else {
		stdin_fd = open("/dev/null", O_RDONLY);
		if (stdin_fd < 0)
			subprocess_child_exit(proc, 1,
					      "E:open(/dev/null):", NULL);

		close(proc->pipe_std[0].rd);
	}

	if (dup2(stdin_fd, 0) < 0 ||
	    dup2(proc->pipe_std[1].wr, 1) < 0 ||
	    dup2(proc->pipe_std[2].wr, 2) < 0)
		subprocess_child_exit(proc, 1, "E:dup2:", NULL);

	close(proc->pipe_ctl.rd);
	close(proc->pipe_std[0].wr);
	close(proc->pipe_std[1].rd);
	close(proc->pipe_std[2].rd);

	if (stdin_fd > 0)
		close(stdin_fd);

	if (proc->pipe_std[1].wr > 0)
		close(proc->pipe_std[1].wr);

	if (proc->pipe_std[2].wr > 0)
		close(proc->pipe_std[2].wr);

	if (cleanup_fn)
		cleanup_fn(priv);

	execvp(argv[0], &argv[0]);
	subprocess_child_exit(proc, 1, "E:execvp:", NULL);
}

static void subprocess_child_terminate(struct subprocess *proc)
{
	sigset_t		mask;
	int			sfd = -1;

	assert(proc->pid != -1);
	assert(proc->is_spawned);

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

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

	return;
}

static bool subprocess_main_start(struct subprocess *proc)
{
	bool		rc;
	char		buf[128];
	ssize_t		l;

	close(proc->pipe_ctl.wr);
	proc->pipe_ctl.wr = -1;

	close(proc->pipe_std[0].rd);
	proc->pipe_std[0].rd = -1;

	close(proc->pipe_std[1].wr);
	proc->pipe_std[1].wr = -1;

	close(proc->pipe_std[2].wr);
	proc->pipe_std[2].wr = -1;

	l = read(proc->pipe_ctl.rd, buf, sizeof buf);
	if (l > 0) {
		fprintf(stderr, "internal error: %.*s\n", (int)l, buf);
		rc = false;
	} else if (l < 0) {
		perror("read(<ctl_pipe>)");
		rc = false;
	} else
		rc = true;

	return rc;
}

bool subprocess_spawn(struct subprocess *proc, int argc, char *argv[],
		      subprocess_child_cleanup_fn *cleanup_fn, void *priv)
{
	bool		rc = false;
	sigset_t	mask;
	sigset_t	old_mask;

	assert(proc->is_init);
	assert(!proc->is_spawned);
	assert(proc->old_chld_mask == 0);

 	sigemptyset(&mask);
 	sigaddset(&mask, SIGCHLD);

	if (sigprocmask(SIG_BLOCK, &mask, &old_mask) == -1) {
 		perror("sigprocmask(<SIG_BLOCK>, <SIGCHLD>)");
 		goto out;
 	}

	proc->old_chld_mask = sigismember(&old_mask, SIGCHLD) ? 1 : -1;

	proc->pid = fork();
	proc->is_spawned = proc->pid >= 0;

	if (proc->pid < 0)
		goto out;
	else if (proc->pid == 0)
		subprocess_child_run(proc, argc, argv, cleanup_fn, priv);
	else if (!subprocess_main_start(proc))
		subprocess_child_terminate(proc);
	else
		rc = true;

out:
	return rc;
}

struct subprocess_run_fds {
	int		epoll;
	int		signal;
	int		timer;

	sigset_t	orig_sigmask;
};

static bool subprocess_run_fds_init(struct subprocess_run_fds *fds,
				    unsigned int timeout)
{
	sigset_t		mask;
	bool			rc = false;

	struct itimerspec const	tm = {
		.it_value = {
			.tv_sec		= timeout,
			.tv_nsec	= 0,
		}
	};

	struct epoll_event	ev = {
		.events	= EPOLLIN
	};

	fds->epoll = -1;
	fds->signal = -1;
	fds->timer = -1;

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	fds->signal = signalfd(-1, &mask, 0);
	if (fds->signal < 0) {
		perror("signalfd()");
		goto out;
	}

	fds->timer = timerfd_create(CLOCK_MONOTONIC, 0);
	if (fds->timer < 0) {
		perror("timerfd_create()");
		goto out;
	}

	if (timerfd_settime(fds->timer, 0, &tm, NULL) < 0) {
		perror("timerfd_settime()");
		goto out;
	}

	fds->epoll = epoll_create(1);
	if (fds->epoll < 0) {
		perror("epoll_create()");
		goto out;
	}

	ev.data.u32 = SUBPROCESS_CB_SOURCE_EXIT;
	if (epoll_ctl(fds->epoll, EPOLL_CTL_ADD, fds->signal, &ev) < 0) {
		perror("epoll_ctl(EPOLL_CTL_ADD, <signal_fd>)");
		goto out;
	}

	ev.data.u32 = SUBPROCESS_CB_SOURCE_TIMEOUT;
	if (epoll_ctl(fds->epoll, EPOLL_CTL_ADD, fds->timer, &ev) < 0) {
		perror("epoll_ctl(EPOLL_CTL_ADD, <timer_fd>)");
		goto out;
	}

	rc = true;

out:
	if (!rc) {
		xclose(fds->epoll);
		xclose(fds->timer);
		xclose(fds->signal);

		/* \hack: use epoll fd to signal unitialized structure */
		fds->epoll = -1;
	}

	return rc;
}

static void subprocess_run_fds_destroy(struct subprocess_run_fds *fds)
{
	if (!fds || fds->epoll == -1)
		return;

	close(fds->epoll);
	close(fds->timer);
	close(fds->signal);

	sigprocmask(SIG_SETMASK, &fds->orig_sigmask, NULL);
}

struct subprocess_epoll_fdinfo {
	int		fd;
	bool		is_out;
};

static int subprocess_mod_epoll(int efd,
				struct subprocess_epoll_fdinfo const info[],
				enum subprocess_cb_source source,
				unsigned long flags, unsigned long old_flags)
{
	int			rc;
	int			fd = info[source].fd;
	unsigned long		changed_flags = flags ^ old_flags;
	struct epoll_event	ev = {
		.events = info[source].is_out ? EPOLLOUT : EPOLLIN,
		.data = {
			.u32 = source,
		}
	};

	if (0)
	printf("%s(%u, %p, %u, %04lx ^ %04lx -> %04lx\n", __func__, efd, info, source,
	       flags, old_flags, changed_flags);

	assert(source < CHAR_BIT * sizeof changed_flags);

	if (!test_bit(source, &changed_flags))
		/* no change */
		rc = 0;
	else if (test_bit(source, &flags))
		rc = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
	else {
		rc = epoll_ctl(efd, EPOLL_CTL_DEL, fd, &ev);
		if (rc < 0 && errno == ENOENT)
			/* ignore error when trying to remove an fd which was
			 * not registered yet */
			rc = 0;
	}

	if (rc < 0)
		perror("epoll_ctl()");

	return rc;
}

static bool subprocess_run_exec_cb(struct subprocess_callbacks const *cb,
				   unsigned long mask,
				   struct subprocess_epoll_fdinfo const info[])
{
	enum subprocess_cb_source	src;
	enum subprocess_cb_source	end_src = SUBPROCESS_CB_SOURCE_EXIT;
	bool				ret = true;

	for (src = SUBPROCESS_CB_SOURCE_MONITOR; mask && src <= end_src; ++src) {
		int		fd;

		if (!test_bit(src, &mask))
			continue;

		clear_bit(src, &mask);

		/* do not handle the special timeout + SIGCHLD events as long
		 * there is normal io */
		if (src < SUBPROCESS_CB_SOURCE_TIMEOUT) {
			end_src = SUBPROCESS_CB_SOURCE_TIMEOUT;
			--end_src;
			fd = info[src].fd;
		} else {
			fd = -1;
			ret = false;
		}

		cb->fn_handle(cb->priv, fd, src);
	}

	return ret;
}

bool subprocess_run(struct subprocess *proc,
		    struct subprocess_callbacks const *cb)
{
	struct subprocess_run_fds	fds;
	bool		ret 		= false;
	unsigned long	old_flags	= ~0Lu; /* signals first run */
	struct subprocess_epoll_fdinfo const	cb_fds[] = {
		[SUBPROCESS_CB_SOURCE_MONITOR] = { cb->fd_monitor, true},
		[SUBPROCESS_CB_SOURCE_STDIN] =	{ proc->pipe_std[0].wr, true },
		[SUBPROCESS_CB_SOURCE_STDOUT] =	{ proc->pipe_std[1].rd, false },
		[SUBPROCESS_CB_SOURCE_STDERR] =	{ proc->pipe_std[2].rd, false },
	};
	unsigned long	hup_mask = 0;


	assert(proc->is_init);
	assert(proc->is_spawned);
	assert(proc->pid != -1);

	assert(cb->fn_step != NULL);
	assert(cb->fn_handle != NULL);

	assert((int)SUBPROCESS_CB_FLAG_MONITOR == (int)SUBPROCESS_CB_SOURCE_MONITOR);
	assert((int)SUBPROCESS_CB_FLAG_STDIN   == (int)SUBPROCESS_CB_SOURCE_STDIN);
	assert((int)SUBPROCESS_CB_FLAG_STDOUT  == (int)SUBPROCESS_CB_SOURCE_STDOUT);
	assert((int)SUBPROCESS_CB_FLAG_STDERR  == (int)SUBPROCESS_CB_SOURCE_STDERR);

	if (cb->fd_monitor != -1)
		set_bit(SUBPROCESS_CB_SOURCE_MONITOR, &hup_mask);

	set_bit(SUBPROCESS_CB_SOURCE_STDIN,  &hup_mask);
	set_bit(SUBPROCESS_CB_SOURCE_STDOUT, &hup_mask);
	set_bit(SUBPROCESS_CB_SOURCE_STDERR, &hup_mask);

	/* \todo: allow to customize timeout */
	if (!subprocess_run_fds_init(&fds, 10))
		/* \todo: signal OSERR */
		goto out;

#if 0
	if (wait4(proc->pid, &proc->exit_status,
		  WNOHANG, &proc->rusage) == proc->pid) {
		proc->pid = -1;
		ret = true;
	}
#endif

	while (!ret) {
		unsigned long		flags;
		int			rc = 0;
		int			nfds;
		struct epoll_event	events[6];
		unsigned long		sources_mask;
		enum subprocess_cb_source	src;

		flags = 0;
		if (old_flags != ~0Lu)
			cb->fn_step(cb->priv, &flags);
		else {
			/* check all fds on first run */
			set_bit(SUBPROCESS_CB_SOURCE_MONITOR, &flags);
			set_bit(SUBPROCESS_CB_SOURCE_STDIN, &flags);
			set_bit(SUBPROCESS_CB_SOURCE_STDOUT, &flags);
			set_bit(SUBPROCESS_CB_SOURCE_STDERR, &flags);
			old_flags = ~flags;
		}

//		printf("%s:%u -> flags=%04lx\n", __func__, __LINE__, flags);

		if (test_bit(SUBPROCESS_CB_FLAG_QUIT, &flags))
			break;

		for (src = SUBPROCESS_CB_SOURCE_MONITOR;
		     src <= SUBPROCESS_CB_SOURCE_STDERR && rc >= 0;
		     ++src) {
			if (cb_fds[src].fd < 0)
				continue;

			if (!test_bit(src, &hup_mask))
				clear_bit(src, &flags);

			rc = subprocess_mod_epoll(fds.epoll, cb_fds,
						  src, flags, old_flags);
		}

		if (rc < 0)
			goto out;

		old_flags = flags;

		nfds = epoll_wait(fds.epoll, events, ARRAY_SIZE(events), -1);
		if (nfds < 0) {
			perror("epoll_wait()");
			break;
		}

		sources_mask = 0;
		while (nfds-- > 0) {
			unsigned long	evs = events[nfds].events;
			src = events[nfds].data.u32;

			if (src > SUBPROCESS_CB_SOURCE_EXIT)
				/* \todo: handle me better! */
				abort();

			set_bit(src, &sources_mask);


			if ((evs == EPOLLHUP) || (evs & EPOLLERR))
				clear_bit(src, &hup_mask);
		}

		if (!subprocess_run_exec_cb(cb, sources_mask, cb_fds))
			ret = true;
	}

	if (proc->pid == -1 || !ret)
		goto out;

	if (wait4(proc->pid, &proc->exit_status,
		  WNOHANG, &proc->rusage) != proc->pid) {
		ret = false;
		goto out;
	}

	proc->pid = -1;
		
		

out:
	subprocess_run_fds_destroy(&fds);

	if (!ret && proc->pid != -1)
		subprocess_child_terminate(proc);

	return ret;
}
