/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
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

#include <minos/minos.h>
#include <minos/console.h>
#include <minos/of.h>

#define MEM_CONSOLE_SIZE	(4096)

static int widx;
static char mem_log_buf[MEM_CONSOLE_SIZE];

#define MEM_CONSOLE_IDX(idx)	(idx & (MEM_CONSOLE_SIZE - 1))

static void mem_console_putc(char ch)
{
	mem_log_buf[MEM_CONSOLE_IDX(widx++)] = ch;
}

static char mem_console_getc(void)
{
	return 0;
}
DEFINE_CONSOLE(mem_console, "mem-console", NULL,
		mem_console_putc, mem_console_getc);

static struct console *console = &__console_mem_console;

struct console *get_console(char *name)
{
	extern unsigned long __console_start;
	extern unsigned long __console_end;
	struct console *console;

	if (!name)
		return NULL;

	section_for_each_item(__console_start, __console_end, console) {
		if (strcmp(console->name, name) == 0)
			return console;
	}

	return NULL;
}

void console_init(char *name)
{
	if (name)
		console = get_console(name);

	if (console->init)
		console->init(NULL);
}

void console_putc(char ch)
{
	console->putc(ch);
}

char console_getc(void)
{
	return console->getc();
}
