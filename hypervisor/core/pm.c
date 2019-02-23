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

int pcpu_can_idle(struct pcpu *pcpu)
{
	if (!sched_can_idle(pcpu))
		return 0;

	if (need_resched())
		return 0;

	return 1;
}

void cpu_idle(void)
{
	unsigned long flags;
	struct pcpu *pcpu = get_cpu_var(pcpu);

	while (1) {
		sched();

		/*
		 * need to check whether the pcpu can go to idle
		 * state to avoid the interrupt happend before wfi
		 */
		local_irq_save(flags);
		if (pcpu_can_idle(pcpu)) {
			pcpu->state = PCPU_STATE_IDLE;
			wfi();
			dsb();
			pcpu->state = PCPU_STATE_RUNNING;
		}
		local_irq_restore(flags);
	}
}
