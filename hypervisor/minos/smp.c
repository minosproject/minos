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
#include <config/config.h>
#include <minos/percpu.h>
#include <minos/platform.h>

extern unsigned char __smp_affinity_id;
uint64_t *smp_affinity_id;

int smp_cpu_up(unsigned long cpu, unsigned long entry)
{
	return platform_cpu_on(cpu, entry);
}

void smp_cpus_up(void)
{
	int i, ret;
	uint64_t affinity;

	flush_cache_all();

	for (i = 1; i < CONFIG_NR_CPUS; i++) {
		affinity = cpuid_to_affinity(i);
		ret = smp_cpu_up(affinity, CONFIG_MINOS_START_ADDRESS);
		if (ret) {
			pr_fatal("failed to bring up cpu-%d\n", i);
			continue;
		}

		pr_info("waiting for cpu-%d up\n", i);
		while (smp_affinity_id[i] == 0)
			cpu_relax();

		pr_debug("cpu-%d is up with affinity id 0x%p\n",
				i, smp_affinity_id[i]);
	}
}

void smp_init(void)
{
	smp_affinity_id = (uint64_t *)&__smp_affinity_id;
	memset(smp_affinity_id, 0, sizeof(uint64_t) * NR_CPUS);
}
