/*	--*- c -*--
 * Copyright (C) 2012 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <fcntl.h>

static int read_all(int fd, void *data, size_t len)
{
	char		*ptr = data;

	while (len>0) {
		ssize_t	l = read(fd, ptr, len);
		if (l>0) {
			ptr += l;
			len -= l;
		} else if (l==0)
			break;
		else if (errno==EINTR)
			continue;
		else {
			perror("read()");
			return -1;
		}
	}

	return len;
}

int main(int argc, char *argv[])
{
	int	fd = open(argv[1], O_RDWR|O_NOCTTY);
	ssize_t	l;
	size_t	rd_len = atoi(argv[3]);
	char	buf[rd_len];

	if (fd < 0) {
		perror("open()");
		return EX_OSERR;
	}

	l = write(fd, argv[2], strlen(argv[2]));
	if (l < 0) {
		perror("write()");
		return EX_OSERR;
	}

	if (l != strlen(argv[2])) {
		fprintf(stderr, "failed to write all data (%zu vs. %zu)\n",
			l, strlen(argv[2]));
		return EX_IOERR;
	}

	if (read_all(fd, buf, rd_len) != 0) {
		perror("read()");
		return EX_IOERR;
	}

	if (l != rd_len) {
		fprintf(stderr, "not all data read (%zu vs. %zu)\n",
			l, rd_len);
		return EX_IOERR;
	}

	printf("%.*s\n", (int)l, buf);
	return EX_OK;
}
