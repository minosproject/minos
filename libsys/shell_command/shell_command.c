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

#include <minos/minos.h>
#include <sys/shell_command.h>

int excute_shell_command(int argc, char **argv)
{
	struct shell_command *cmd;
	extern unsigned long __shell_command_start;
	extern unsigned long __shell_command_end;

	if ((argc == 0) || (argv[0] == NULL))
		return -EINVAL;

	section_for_each_item(__shell_command_start,
				__shell_command_end, cmd) {
		if (strcmp(argv[0], cmd->name) == 0) {
			if (cmd->hdl == NULL)
				return -ENOENT;

			return cmd->hdl(argc, argv);
		}
	}

	return -ENOENT;
}
