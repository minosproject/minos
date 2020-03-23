/*
 * Copyright (C) 2017 - 2020 Min Le (lemin9538@gmail.com)
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

#include <minos/shell_command.h>
#include <minos/print.h>

static int clear_cmd(int argc, char **argv)
{
	printf("\x1b[2J\x1b[H");
	return 0;
}
DEFINE_SHELL_COMMAND(clear, "clear", "Clear the screec", clear_cmd, 0);
