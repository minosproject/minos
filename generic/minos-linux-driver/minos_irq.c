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

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/irqdomain.h>

#define SPI_IRQ_BASE 32

static struct irq_domain *mdomain;

static struct irq_domain *of_find_irq_host(void)
{
	int ret;
	uint32_t phandle;
	struct device_node *node;

	ret = of_property_read_u32(of_root, "interrupt-parent", &phandle);
	if (ret || !phandle) {
		pr_err("failed to get the phandle of the interrupt controller\n");
		return NULL;
	}

	node = of_find_node_by_phandle(phandle);
	if (!node)
		return NULL;

	return irq_find_host(node);
}

int get_dynamic_virq(int irq)
{
	WARN_ONCE(!mdomain, "irq domain not init\n");

	if (!mdomain)
		return 0;

	return irq_find_mapping(mdomain, irq);
}

static int minos_irq_init(void)
{
	mdomain = of_find_irq_host();
	if (!mdomain)
		pr_err("can not find the root irq domain\n");
	else
		pr_info("find irq domain %s\n", mdomain->name);

	return 0;
}
arch_initcall(minos_irq_init);
