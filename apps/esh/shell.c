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

#include <minos/app.h>
#include <minos/console.h>
#include <minos/compiler.h>
#include <minos/shell_command.h>
#include <minos/tty.h>
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
	pesh->tty = NULL;
}

static int shell_cmd_tty(int argc, char **argv)
{
	uint32_t id;

	if (argc < 3) {
		printf("invalid argument\n");
		return -EINVAL;
	}

	if (strcmp(argv[1], "attach") == 0) {
		id = atoi(argv[2]);

		pesh->tty = open_tty(0xabcd0000 | id);
		if (!pesh->tty) {
			printf("no such tty\n");
			return -EINVAL;
		}
	} else {
		printf("unsupport action now\n");
	}

	return 0;
}
DEFINE_SHELL_COMMAND(tty, "tty", "tty related command", shell_cmd_tty);

static void shell_task(void *data)
{
	char ch;

	pesh = esh_init();
	esh_register_command(pesh, esh_excute_command);
	esh_register_print(pesh, __esh_putc);

	esh_rx(pesh, '\n');

	pesh->tty = open_tty(0xabcd0000);

	/* clear the fifo */
	while (console_getc());
	
	while (1) {
		for (; ;) {
			ch = console_getc();
			if (ch <= 0)
				break;

			if (pesh->tty) {
				if (ch == 27)	/* esc key */
					shell_detach_tty();
				else
					pesh->tty->ops->put_char(pesh->tty, ch);
			} else {
				if (ch == '\r')
					ch = '\n';
				esh_rx(pesh, ch);
			}
		}

		msleep(10);
	}
}
DEFINE_REALTIME_TASK("shell_task", shell_task,
		NULL, CONFIG_SHELL_TASK_PRIO, 4096, 0);
