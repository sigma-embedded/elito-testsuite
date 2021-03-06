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

#ifndef H_ENSC_TESTSUITE_SRC_UTIL_H
#define H_ENSC_TESTSUITE_SRC_UTIL_H

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>

#define ARRAY_SIZE(_a)		(sizeof(_a) / sizeof (_a)[0])

static inline void xclose(int fd)
{
	if (fd >= 0)
		close(fd);
}

static inline int set_cloexec(int fd, bool ena)
{
	unsigned long	flags = fcntl(fd, F_GETFD);

	if (ena)
		flags |= FD_CLOEXEC;
	else
		flags &= ~FD_CLOEXEC;

	return fcntl(fd, F_SETFD, flags);
}

static inline bool write_all(int fd, void const *buf, size_t len)
{
	while (len > 0) {
		ssize_t	l = write(fd, buf, len);
		if (l >= 0) {
			buf += l;
			len -= l;
		} else if (errno == EINTR)
			continue;
		else {
			perror("write()");
			break;
		}
	}

	return len == 0;
}

static inline bool test_bit(unsigned int num, unsigned long const *mask)
{
	return (mask[num / (CHAR_BIT * sizeof mask[0])] &
		(1Lu << (num % (CHAR_BIT * sizeof mask[0]))));
}

static inline void set_bit(unsigned int num, unsigned long *mask)
{
	mask[num / (CHAR_BIT * sizeof mask[0])] |=
		(1Lu << (num % (CHAR_BIT * sizeof mask[0])));
}

static inline void clear_bit(unsigned int num, unsigned long *mask)
{
	mask[num / (CHAR_BIT * sizeof mask[0])] &=
		~(1Lu << (num % (CHAR_BIT * sizeof mask[0])));
}

#endif	/* H_ENSC_TESTSUITE_SRC_UTIL_H */
