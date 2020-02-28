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
#include "esh.h"

static void esh_putc(esh_t *esh, char c, void *arg)
{
	console_putc(c);
}

static void esh_excute_command(esh_t *esh,
		int argc, char **argv, void *arg)
{
	int ret;

	ret = excute_shell_command(argc, argv);
	if (ret == -ENOENT)
		printf("Command \'%s\' not found\n", argv[0]);
}

void shell_task(void *data)
{
	char ch;

	esh_t *esh = esh_init();
	esh_register_command(esh, esh_excute_command);
	esh_register_print(esh, esh_putc);

	esh_rx(esh, '\n');

	while (1) {
		ch = console_getc();
		if (ch > 0) {
			if (ch == '\r')
				ch = '\n';

			esh_rx(esh, ch);
		}

		msleep(10);
	}
}
DEFINE_REALTIME_TASK("shell_task", shell_task,
		NULL, CONFIG_SHELL_TASK_PRIO, 4096, 0);
