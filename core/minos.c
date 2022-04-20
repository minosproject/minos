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
#include <minos/percpu.h>
#include <minos/irq.h>
#include <minos/mm.h>
#include <asm/arch.h>
#include <minos/pm.h>
#include <minos/init.h>
#include <minos/sched.h>
#include <minos/smp.h>
#include <minos/atomic.h>
#include <minos/softirq.h>
#include <minos/platform.h>
#include <config/version.h>
#include <minos/of.h>
#include <minos/ramdisk.h>

extern void cpu_idle(void);
extern int allsymbols_init(void);
extern void platform_init(void);
extern int create_idle_task(void);
extern int load_root_service(void);
extern int kernel_vspace_init(void);
extern void mm_init(void);

#ifdef CONFIG_VIRT
#include <virt/virt.h>
#endif

void boot_main(void)
{
	allsymbols_init();
	percpu_init(0);

	pr_notice("Minos %s\n", MINOS_VERSION_STR);

	ASSERT(smp_processor_id() == 0);

	kernel_vspace_init();
	mm_init();

#ifdef CONFIG_DEVICE_TREE
	of_init_bootargs();
#endif

	early_init();
	early_init_percpu();

	arch_init();
	arch_init_percpu();

	platform_init();
	irq_init();

#ifdef CONFIG_SMP
	smp_init();
#endif
	subsys_init();
	subsys_init_percpu();

	module_init();
	module_init_percpu();

	sched_init();
	local_sched_init();

	device_init();
	device_init_percpu();

	ramdisk_init();
	create_idle_task();

#ifdef CONFIG_SMP
	smp_cpus_up();
#endif

#ifdef CONFIG_VIRT
	virt_init();
#endif
	cpu_idle();
}

void boot_secondary(int cpuid)
{
	pr_notice("cpu-%d is up\n", cpuid);

	/*
	 * need wait for all cpus up then excuted below
	 * task, otherwise the mem content hold by different
	 * cpu may be different because the cache issue
	 *
	 * eg: the cpu1 called create_idle_task and the
	 * idle task is created sucessfully but at the same
	 * time the cpu2 is powered off
	 *
	 * waitting for all the cpu power on
	 */
	while (!is_cpus_all_up())
		mb();

	early_init_percpu();

	arch_init_percpu();

	irq_secondary_init();

	subsys_init_percpu();

	module_init_percpu();

	local_sched_init();

	device_init_percpu();

	create_idle_task();

	cpu_idle();
}
