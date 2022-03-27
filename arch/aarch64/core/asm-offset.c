/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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

#include <minos/kbuild.h>
#include <minos/percpu.h>
#include <minos/task_def.h>

#define __NO_STUBS	1

int main(void)
{
	DEFINE(TASK_INFO_SIZE, sizeof(struct task_info));
	DEFINE(TASK_INFO_FLAGS_OFFSET, offsetof(struct task_info, flags));
	DEFINE(PCPU_SIZE, sizeof(struct pcpu));
	DEFINE(PCPU_ID_OFFSET, offsetof(struct pcpu, pcpu_id));
	DEFINE(PCPU_STACK_OFFSET, offsetof(struct pcpu, stack));
	DEFINE(TASK_SIZE, sizeof(struct task));
	DEFINE(TASK_STACK_OFFSET, offsetof(struct task, stack_base));
	DEFINE(PCPU_CURRENT_TASK, offsetof(struct pcpu, running_task));
	DEFINE(GP_REGS_SIZE, sizeof(gp_regs));

	return 0;
}
