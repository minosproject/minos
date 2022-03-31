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
#include <minos/bootarg.h>
#include <minos/console.h>
#include <minos/flag.h>

#ifdef CONFIG_VIRT
extern void start_all_vm(void);
#endif

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
		(is_list_empty(&pcpu->stop_list));
}

static int __init_task(void *main)
{
#ifdef CONFIG_VIRT
	printf("\n\nStarting all VMs\n\n");
	start_all_vm();
#else
	printf("\n\nHello Minos\n\n");
#endif
	return 0;
}
weak_alias(__init_task, init_task);

static void pcpu_release_task(struct pcpu *pcpu)
{
	unsigned long flags;
	struct task *task;

	local_irq_save(flags);

	while (!is_list_empty(&pcpu->stop_list)) {
		task = list_first_entry(&pcpu->stop_list, struct task, stat_list);
		list_del(&task->stat_list);
		do_release_task(task);
	}

	local_irq_restore(flags);
}

static int kworker_task(void *data)
{
	struct pcpu *pcpu = get_pcpu();
	flag_t flag;

	pcpu->kworker = current;
	flag_init(&pcpu->kworker_flag, 0);

	for (;;) {
		flag = flag_pend(&pcpu->kworker_flag, KWORKER_FLAG_MASK,
				FLAG_WAIT_SET_ANY | FLAG_CONSUME, 0);
		if (flag == 0) {
			pr_err("kworker: no event trigger\n");
			continue;
		}

		if (flag & KWORKER_TASK_RECYCLE)
			pcpu_release_task(pcpu);
	}

	return 0;
}

static void start_system_task(void)
{
	int cpu = smp_processor_id();
	struct task *task;
	char name[32];

	pr_notice("create kworker task...\n");
	sprintf(name, "kworker/%d", cpu);
	task = create_task(name, kworker_task, 0x2000,
			OS_PRIO_DEFAULT_1, cpu, 0, NULL);
	ASSERT(task != NULL);

	if (cpu == 0) {
		pr_notice("create init task...\n");
		task = create_task("init", init_task, 0x2000,
				OS_PRIO_SYSTEM, -1, 0, NULL);
		if (!task)
			pr_err("create init task failed\n");
	}
}

void cpu_idle(void)
{
	struct pcpu *pcpu = get_pcpu();

	set_os_running();
	local_irq_enable();

	start_system_task();

	while (1) {
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

		sched();
	}
}
