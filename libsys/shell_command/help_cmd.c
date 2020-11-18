// SPDX-License-Identifier: GPL-2.0

#include <minos/minos.h>
#include <minos/shell_command.h>

extern unsigned long __shell_command_start;
extern unsigned long __shell_command_end;

static int help_cmd(int argc, char **argv)
{
    struct shell_command *cmd;
    char *spaces = "       ";

    section_for_each_item(__shell_command_start, __shell_command_end, cmd) {
        printf("%s%s - %s\n",
               cmd->name,
               spaces + (MIN(strlen(cmd->name), strlen(spaces))),
               cmd->cmd_info);
    }

    return 0;
}

DEFINE_SHELL_COMMAND(help, "help", "print command description", help_cmd, 0);
