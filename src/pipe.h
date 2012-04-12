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

#ifndef H_ENSC_TESTSUITE_SRC_PIPE_H
#define H_ENSC_TESTSUITE_SRC_PIPE_H

#include <unistd.h>

struct pipe {
	int	rd;
	int	wr;
};

static inline int pipe_create(struct pipe *p)
{
	int		p_[2];
	int		rc = pipe(p_);

	if (rc < 0) {
		p->rd = -1;
		p->wr = -1;
	} else {
		p->rd = p_[0];
		p->wr = p_[1];
	}

	return rc;
}

static inline void pipe_close(struct pipe *p)
{
	if (p->rd != -1)
		close(p->rd);

	if (p->wr != -1)
		close(p->wr);
}

#endif	/* H_ENSC_TESTSUITE_SRC_PIPE_H */
