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

void cpu_idle(void)
{
	struct pcpu *pcpu = get_cpu_var(pcpu);

	/*
	 * if interrupt is happend when after the
	 * pcpu state is setted ? TBD
	 */
	pcpu->state = PCPU_STATE_IDLE;
	wfi();
	pcpu->state = PCPU_STATE_RUNNING;
}

void cpu_idle_task()
{
	//printf("cpu idle for cpu-%d\n", get_cpu_id());
	//wfi();
}
