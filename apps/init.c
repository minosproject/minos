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

#include <minos/console.h>
#include <minos/shell_command.h>
#include <minos/bootarg.h>
#include <minos/print.h>
#include <minos/task.h>

#ifdef CONFIG_VIRT
#include <virt/virt.h>
#endif

static int __skip_vm_boot;

static void skip_vm_boot(void)
{
#ifdef CONFIG_VIRT

#ifdef CONFIG_SHELL
	uint32_t wait;
	char str[8];
	int i;

	bootarg_parse_uint("bootwait", &wait);
	if (wait > 0) {
		printf("\nPress any key to stop vm startup: %d ", wait);
		for (i = 0; i < wait; i++) {
			printf("\b\b%d ", wait - i);
			if (console_gets(str, 8, 1000) > 0) {
				__skip_vm_boot = 1;
				break;
			}
		}
	}

	if (!__skip_vm_boot) {
		printf("\b\b ");
		printf("\n");
	}
#endif

	if (!__skip_vm_boot)
		start_all_vm();
#endif
}

static void start_shell_task(void)
{
#ifdef CONFIG_SHELL
	extern int shell_task(void *data);
	create_task("shell_task", shell_task,
			0x2000, OS_PRIO_SYSTEM, -1, 0, NULL);
#endif
}

int init_task(void *data)
{
	skip_vm_boot();
	start_shell_task();

	return 0;
}
