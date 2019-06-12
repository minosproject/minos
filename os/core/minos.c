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

extern void softirq_init(void);
extern void init_timers(void);
extern int virt_init(void);
extern void cpu_idle(void);
extern void sched_tick_enable(unsigned long exp);
extern void vmm_init(void);
extern void bootmem_init(void);
extern int allsymbols_init(void);
extern void platform_init(void);
extern int create_idle_task(void);

struct list_head hook_lists[MINOS_HOOK_TYPE_UNKNOWN];

static void hooks_init(void)
{
	int i;

	for (i = 0; i < MINOS_HOOK_TYPE_UNKNOWN; i++)
		init_list(&hook_lists[i]);
}

int register_hook(hook_func_t fn, enum hook_type type)
{
	struct hook *hook;

	if ((fn == NULL) || (type >= MINOS_HOOK_TYPE_UNKNOWN)) {
		pr_error("Hook info is invaild\n");
		return -EINVAL;
	}

	hook = malloc(sizeof(*hook));
	if (!hook)
		return -ENOMEM;

	memset(hook, 0, sizeof(*hook));
	hook->fn = fn;

	list_add_tail(&hook_lists[type], &hook->list);

	return 0;
}

int do_hooks(void *item, void *context, enum hook_type type)
{
	int err = 0;
	struct hook *hook;

	list_for_each_entry(hook, &hook_lists[type], list)
		err += hook->fn(item, context);

	return err;
}

void boot_main(void *setup_data)
{
	allsymbols_init();
	percpus_init();

	pr_info("Starting Minos %s\n", MINOS_VERSION_STR);

	if (smp_processor_id() != 0)
		panic("boot_main : cpu is not cpu0");

	/*
	 * at the early stage when the memory mangement
	 * has not been finish init, system can using
	 * alloc_boot_mem or alloc_boot_page to get
	 * free mem or free pages
	 */
	bootmem_init();

	early_init(setup_data);
	early_init_percpu();

	mm_init();

	hooks_init();

	arch_init();
	arch_init_percpu();

	pcpus_init();
	platform_init();
	irq_init();
	smp_init();
	softirq_init();
	init_timers();

	subsys_init();
	subsys_init_percpu();

	module_init();
	module_init_percpu();

	sched_init();
	local_sched_init();

	virt_init();
	vmm_init();

	device_init();
	device_init_percpu();

	create_idle_task();

	smp_cpus_up();

	set_os_running();
	local_irq_enable();

	cpu_idle();
	panic("Should Not be here\n");
}

void boot_secondary(void)
{
	uint32_t cpuid = smp_processor_id();

	pr_info("cpu-%d is up\n", cpuid);

	early_init_percpu();

	arch_init_percpu();

	irq_secondary_init();

	subsys_init_percpu();

	module_init_percpu();

	local_sched_init();

	device_init_percpu();

	create_idle_task();

	set_os_running();
	local_irq_enable();

	cpu_idle();
}
