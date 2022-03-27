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
#include <asm/arch.h>
#include <minos/sched.h>
#include <minos/platform.h>
#include <minos/irq.h>
#include <minos/mm.h>
#include <minos/of.h>
#include <minos/task.h>
#include <minos/flag.h>

void system_reboot(void)
{
	if (platform->system_reboot)
		platform->system_reboot(0, NULL);

	panic("can not reboot system now\n");
}

void system_shutdown(void)
{
	if (platform->system_shutdown)
		platform->system_shutdown();

	panic("cant not shutdown system now\n");
}

int system_suspend(void)
{
	if (platform->system_suspend)
		platform->system_suspend();

	wfi();

	return 0;
}

static inline bool pcpu_can_idle(struct pcpu *pcpu)
{
	return (pcpu->local_rdy_grp == (1 << OS_PRIO_IDLE)) &&
			(is_list_empty(&pcpu->stop_list) &&
			(is_list_empty(&pcpu->die_process)));
}

static void do_pcpu_cleanup_work(struct pcpu *pcpu)
{
	extern void release_thread(struct task *task);
	struct task *task;

	/*
	 * the race may happen when other task in this pcpu
	 * call stop(), so need to disable the preempt to avoid
	 * the idle task preempt by other task here. currently
	 * the stop list will only write when the task request stop
	 * so do not need to diable the interrupt.
	 */
	for (; ;) {
		task = NULL;
		preempt_disable();
		if (!is_list_empty(&pcpu->stop_list)) {
			task = list_first_entry(&pcpu->stop_list,
					struct task, stat_list);
			list_del(&task->stat_list);
		}
		preempt_enable();

		if (!task)
			break;

		do_release_task(task);
	}
}

static int init_task(void *main)
{
#ifdef CONFIG_SHELL
	int i;
	char *tty = NULL;
	int skip_vm_boot = 0;
	uint32_t wait = 0;
	unsigned long timeout;
	extern int shell_task(void *data);
#endif

	/*
	 * first check whether need to stop to start all
	 * VM automaticly if the shell is enabled, if the
	 * shell is enabled, provide a debug mode to do some
	 * debuging for VM
	 */
#ifdef CONFIG_SHELL
#ifdef CONFIG_VIRT
	bootarg_parse_uint("bootwait", &wait);
	if (wait > 0) {
		printf("\nPress any key to stop vm startup: %d ", wait);
		for (i = 0; i < wait; i++) {
			timeout = NOW() + SECONDS(1);

			printf("\b\b%d ", wait - i);

			while (NOW() < timeout) {
				if (console_getc() > 0) {
					skip_vm_boot = 1;
					break;
				}
			}

			if (skip_vm_boot)
				break;
		}
	}

	if (!skip_vm_boot) {
		printf("\b\b0 ");
		printf("\n");
		start_all_vm();
	}
#endif
	if (!skip_vm_boot)
		bootarg_parse_string("tty", &tty);

	create_task("shell_task", shell_task,
			tty, CONFIG_SHELL_TASK_PRIO, 4096, 0);
#else
#ifdef CONFIG_VIRT
	extern void start_all_vm(void);
	start_all_vm();
#endif
#endif

	return 0;
}


void cpu_idle(void)
{
	struct pcpu *pcpu = get_pcpu();

	set_os_running();
	local_irq_enable();

	if (pcpu->pcpu_id == 0) {
		pr_notice("create init task for system\n");
		create_task("init", init_task, 0x2000,
				OS_PRIO_DEFAULT, -1, 0, NULL);
	}

	while (1) {
		do_pcpu_cleanup_work(pcpu);

		/*
		 * need to check whether the pcpu can go to idle
		 * state to avoid the interrupt happend before wfi
		 */
		while (!need_resched() && pcpu_can_idle(pcpu)) {
			local_irq_disable();
			if (pcpu_can_idle(pcpu)) {
				pcpu->state = PCPU_STATE_IDLE;
				wfi();
				nop();
				pcpu->state = PCPU_STATE_RUNNING;
			}
			local_irq_enable();
		}

		set_need_resched();
		sched();
	}
}
