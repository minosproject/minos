/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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
#include <minos/arch.h>
#include <minos/platform.h>
#include <minos/of.h>

extern unsigned char __platform_start;
extern unsigned char __platform_end;
struct platform *platform = NULL;

void platform_set_to(const char *name)
{
	struct platform **pp;
	struct platform *p;

	section_for_each_item(__platform_start, __platform_end, pp) {
		p = *pp;
		if (strcmp(p->name, name) == 0) {
			platform = p;
			break;
		}
	}
}

int platform_iomem_valid(unsigned long addr)
{
	if (platform->iomem_valid)
		return platform->iomem_valid(addr);

	return 1;
}

void platform_init(void)
{
	if (platform->platform_init)
		platform->platform_init();
}

static int platform_early_init(void)
{
	/* check whether the platform has been set
	 * by the arch code in the early boot stage
	 */
	if (platform == NULL)
		panic("no platform found ...\n");

	if (platform->parse_mem_info)
		platform->parse_mem_info();

	return 0;
}
early_initcall(platform_early_init);
