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
#include <sys/resource.h>


typedef int			subprocess_pipes[2];

struct subprocess {
	bool			is_interactive;

	pid_t			pid;
	subprocess_pipes	pipe_ctl;
	subprocess_pipes	pipe_std[3];

	struct rusage		rusage;

	bool			is_init;
	bool			is_spawned;
};

bool subprocess_init(struct subprocess *proc, bool is_interactive);
void subprocess_destroy(struct subprocess *proc);

bool subprocess_spawn(struct subprocess *proc, int argc, char *argv[]);

#endif	/* H_ENSC_TESTSUITE_SRC_SUBPROCESS_H */
