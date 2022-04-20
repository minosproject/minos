/*
 * Copyright (C) 2020 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/console.h>
#include <minos/compiler.h>
#include <minos/shell_command.h>
#include <minos/tty.h>
#include <minos/print.h>
#include <minos/errno.h>
#include <minos/string.h>
#include <minos/bootarg.h>

#include "esh.h"
#include "esh_internal.h"

static struct esh *pesh;

static void __esh_putc(struct esh *esh, char c, void *arg)
{
	console_putc(c);
}

static void esh_excute_command(struct esh *esh,
		int argc, char **argv, void *arg)
{
	int ret;

	ret = excute_shell_command(argc, argv);
	if (ret == -ENOENT)
		printf("Command \'%s\' not found\n", argv[0]);
}

static void shell_detach_tty(void)
{
	close_tty(pesh->tty);
	printf("\nDetach tty: %s\n", pesh->tty->name);
	pesh->tty = NULL;
}

static int shell_cmd_tty(int argc, char **argv)
{
	if (argc > 2 && strcmp(argv[1], "attach") == 0) {
		printf("Attach tty: %s press any key to active the console\n",
				argv[2]);
		pesh->tty = open_tty(argv[2]);
		if (!pesh->tty) {
			printf("no such tty\n");
			return -EINVAL;
		}
	} else {
		printf("unsupport action now\n");
	}

	return 0;
}
DEFINE_SHELL_COMMAND(tty, "tty", "tty related command",
		shell_cmd_tty, 2);

int shell_task(void *data)
{
	char buf[32];
	char ch;
	int i, copy;
	char *tty = NULL;

	pesh = esh_init();
	esh_register_command(pesh, esh_excute_command);
	esh_register_print(pesh, __esh_putc);

	esh_rx(pesh, '\n');
	while (console_gets(buf, 32, 0) != 0);

	if ((long)data == 0) {
		i = bootarg_parse_string("tty", &tty);
		if (!i && tty && !strncmp(tty, "vm", 2)) {
			printf("\nAttach tty: %s\n", tty);
			pesh->tty = open_tty(tty);
		}
	}

	for (; ;) {
		copy = console_gets(buf, 32, -1);
		for (i = 0; i < copy; i++) {
			ch = buf[i];
			if (ch <= 0)
				continue;
			if (pesh->tty) {
				/*
				 * use Ctrl-\ to exit the vm tty
				 */
				if (ch == 28) {
					shell_detach_tty();
					esh_rx(pesh, '\n');
				} else {
					pesh->tty->ops->put_char(pesh->tty, ch);
				}
			} else {
				if (ch == '\r')
					ch = '\n';
				esh_rx(pesh, ch);
			}
		}
	}

	return 0;
}
