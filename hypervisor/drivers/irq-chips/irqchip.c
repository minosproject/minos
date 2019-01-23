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
#include <asm/bcm_irq.h>
#include <minos/device_id.h>
#include <minos/of.h>

int bcm2836_xlate_irq(struct device_node *node,
		uint32_t *intspec, unsigned int intsize,
		uint32_t *hwirq, unsigned long *type)
{
	int phandle;

	phandle = of_get_phandle(node);
	if (phandle == 3) {
		if (intsize != 2)
			return -EINVAL;

		/* workaroud for the bcm2835 irq */
		if (intspec[0] == 8)
			return -EINVAL;

		*hwirq = intspec[0] + 16;
		*type = intspec[1];
		return 0;
	}

	if (intsize == 1) {
		if (intspec[0] < 16) {
			*hwirq = intspec[0] + 16;
			*type = 0;
			return 0;
		}
		return -EINVAL;
	} else if (intsize == 2) {
		if (intspec[0] >= NR_BANKS)
			return -EINVAL;
		if (intspec[1] >= IRQS_PER_BANK)
			return -EINVAL;

		*hwirq = MAKE_HWIRQ(intspec[0], intspec[1]);
		*type = 0;
		return 0;
	} else
		return -EINVAL;
}

int gic_xlate_irq(struct device_node *node,
		uint32_t *intspec, unsigned int intsize,
		uint32_t *hwirq, unsigned long *type)
{
	if (intsize != 3)
		return -EINVAL;

	if (intspec[0] == 0)
		*hwirq = intspec[1] + 32;
	else if (intspec[0] == 1) {
		if (intspec[1] >= 16)
			return -EINVAL;
		*hwirq = intspec[1] + 16;
	} else
		return -EINVAL;

	*type = intspec[2];
	return 0;
}
