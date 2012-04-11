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


#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <getopt.h>
#include <sysexits.h>

#include "subprocess.h"
#include "util.h"

/* {{{ cli options */
#define CMD_HELP		0x8000
#define CMD_VERSION		0x8001
#define CMD_SKIP		's'
#define CMD_FAIL		'f'
#define CMD_INTERACTIVE		'I'

static struct option const	CMDLINE_OPTIONS[] = {
  { "help",        no_argument,        0, CMD_HELP },
  { "version",     no_argument,        0, CMD_VERSION },
  { "skip",        required_argument,  0, CMD_SKIP },
  { "fail",        no_argument,	       0, CMD_FAIL },
  { "interactive", no_argument,	       0, CMD_INTERACTIVE },
  { 0,0,0,0 }
};

struct cmdline_options {
	bool		is_fail;
	bool		is_interactive;
	char const	*skip_reason;
};
/* }}} cli options */

static void show_help(void)
{
	/* \todo */
	exit(0);
}

static void show_version(void)
{
	/* \todo */
	exit(0);
}

static int run_program(struct cmdline_options const *opts,
		       int argc, char *argv[])
{
	struct subprocess	proc;
	int			rc;

	rc = EX_OSERR;
	if (!subprocess_init(&proc, opts->is_interactive))
		goto out;

	if (!subprocess_spawn(&proc, argc, argv))
		goto out;
		

out:
	subprocess_destroy(&proc);
	
	return rc;
}

int main(int argc, char *argv[])
{
	struct cmdline_options		opts = {
		.is_interactive	= false,
	};

	/* cmdline parsing */
	while (1) {
		int	c = getopt_long(argc, argv, "+s:f",
					CMDLINE_OPTIONS, 0);

		switch (c) {
		case CMD_HELP		:  show_help();
		case CMD_VERSION	:  show_version();
		case CMD_FAIL 		:  opts.is_fail = true; break;
		case CMD_SKIP		:  opts.skip_reason = optarg; break;
		default:
			fprintf(stderr, "Try '--help' for more information\n");
			return EX_USAGE;
		}
	}

	run_program(&opts, argc - optind, &argv[optind]);
	execvp(argv[1], &argv[1]);
	return EXIT_FAILURE;
}
