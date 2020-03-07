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

#include <minos/minos.h>
#include <minos/list.h>
#include <minos/of.h>
#include <virt/vm.h>
#include <virt/vdev.h>
#include <virt/virq_chip.h>
#include <minos/mm.h>
#include <virt/vmm.h>
#include <minos/irq.h>
#include <virt/vmbox.h>
#include <minos/platform.h>
#include <virt/resource.h>

static void *varm_timer_init(struct vm *vm, struct device_node *node)
{
	int ret;
	uint32_t irq;
	unsigned long flags;

	/*
	 * for armv8 need to use the virtual timer as the system
	 * ticks, here we get the virtual timer's irq number, the
	 * virtual timer's irq num will with the index 2
	 */
	ret = vm_get_device_irq_index(vm, node, &irq, &flags, 2);
	if (ret || (irq > 32)) {
		pr_err("can not find virtual timer for VM\n");
		return  NULL;
	}

	pr_info("virtual arch timer irq: %d\n", irq);
	vm->vtimer_virq = irq;

	return NULL;
}
VDEV_DECLARE(varm_timer, arm_arch_timer_match_table, varm_timer_init);
