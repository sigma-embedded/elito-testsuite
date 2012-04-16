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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sysexits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
	int		fd;
	struct stat	st;

	if (stat(argv[1], &st) < 0) {
		fprintf(stderr, "stat(%s): %s\n", argv[1], strerror(errno));
		return EX_OSERR;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(%s): %s\n", argv[1], strerror(errno));
		return EX_OSERR;
	}
	close(fd);

	return EX_OK;
}
