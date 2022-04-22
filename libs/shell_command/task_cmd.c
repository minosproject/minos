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

#include <minos/task.h>
#include <minos/shell_command.h>
#include <virt/vm.h>

static char *state_str[7] = {
	"Running",
	"  Ready",
	"   Wait",
	" Waking",
	"Suspend",
	"Stopped",
	"  Wrong",
};

static inline char *get_state_str(struct task *task)
{
	switch (task->state) {
	case TASK_STATE_RUNNING:
		return state_str[0];
	case TASK_STATE_READY:
		return state_str[1];
	case TASK_STATE_WAIT_EVENT:
		return state_str[2];
	case TASK_STATE_WAKING:
		return state_str[3];
	case TASK_STATE_SUSPEND:
		return state_str[4];
	case TASK_STATE_STOP:
		return state_str[5];
	}

	return state_str[6];
}

static void dump_task_info(struct task *task)
{
	char vm_str[8] = {0};
	struct vcpu *vcpu;

	if (task_is_vcpu(task)) {
		vcpu = (struct vcpu *)task->pdata;
		sprintf(vm_str, "vm-%d/", vcpu->vm->vmid);
	}

	printf("%4d %3d %s %s%s\n", task->tid, task->cpu,
			get_state_str(task), vm_str, task->name);
}

static int ps_cmd(int argc, char **argv)
{
	printf(" PID CPU   STATE NAME\n");
	os_for_all_task(dump_task_info);

	return 0;
}
DEFINE_SHELL_COMMAND(ps, "ps", "List all task information", ps_cmd, 0);
