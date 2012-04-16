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

#ifndef H_ENSC_TESTSUITE_SRC_SUBPROCESS_H
#define H_ENSC_TESTSUITE_SRC_SUBPROCESS_H

#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>

#include "pipe.h"

struct subprocess {
	bool			is_interactive;
	unsigned int		timeout; /* modify directly! */

	pid_t			pid;
	struct pipe		pipe_ctl;
	struct pipe		pipe_std[3];

	struct rusage		rusage;
	int			exit_status;

	sigset_t		old_mask;

	int			old_chld_mask;
	bool			is_init;
	bool			is_spawned;
};

enum subprocess_cb_flags {
	SUBPROCESS_CB_FLAG_MONITOR,
	SUBPROCESS_CB_FLAG_STDIN,
	SUBPROCESS_CB_FLAG_STDOUT,
	SUBPROCESS_CB_FLAG_STDERR,

	SUBPROCESS_CB_FLAG_QUIT,
};

enum subprocess_cb_source {
	SUBPROCESS_CB_SOURCE_MONITOR,
	SUBPROCESS_CB_SOURCE_STDIN,
	SUBPROCESS_CB_SOURCE_STDOUT,
	SUBPROCESS_CB_SOURCE_STDERR,

	SUBPROCESS_CB_SOURCE_TIMEOUT,
	SUBPROCESS_CB_SOURCE_EXIT,
};

typedef void	subprocess_cb_step(void *priv, unsigned long *flags); 
typedef void	subprocess_cb_fn(void *priv, int fd, enum subprocess_cb_source);
typedef void	subprocess_child_cleanup_fn(void *priv);

struct subprocess_callbacks {
	int			fd_monitor;

	subprocess_cb_step	*fn_step;
	subprocess_cb_fn	*fn_handle;

	void			*priv;
};

bool subprocess_init(struct subprocess *proc, bool is_interactive);
void subprocess_destroy(struct subprocess *proc);

bool subprocess_spawn(struct subprocess *proc, int argc, char *argv[],
		      subprocess_child_cleanup_fn *cleanup_fn, void *priv);
bool subprocess_run(struct subprocess *proc,
		    struct subprocess_callbacks const *cb);

#endif	/* H_ENSC_TESTSUITE_SRC_SUBPROCESS_H */
