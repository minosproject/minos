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

DEFINE_PER_CPU(uint64_t, cpu_id);

extern unsigned char __smp_hoding_pen;
uint64_t *smp_holding_pen;

uint32_t smp_get_cpuid(uint64_t mpidr_id)
{
	uint8_t aff0, aff1;

	mpidr_id &= MPIDR_ID_MASK;
	aff0 = (uint8_t)(mpidr_id);
	aff1 = (uint8_t)(mpidr_id >> 8);

	/*
	 * now assume there are only one cluster
	 * this is different on each platform, on
	 * fvp there is one cluster, so the aff0 value
	 * is the cpuid
	 */
	return (uint32_t)(aff1 * 4 + aff0);
}

int smp_cpu_up(uint64_t mpidr_id)
{
	uint32_t smp_id;

	smp_id = smp_get_cpuid(mpidr_id);
	if (smp_id > CONFIG_NR_CPUS)
		return -EINVAL;

	smp_holding_pen[smp_id] = mpidr_id;
	return 0;
}

int wait_cpus_up(void)
{
	int i;
	int ok;

	while (1) {
		ok = 0;
		for (i = 1; i < CONFIG_NR_CPUS; i++) {
			if (smp_holding_pen[i] != 0xffff) {
				ok = 1;
				break;
			}
		}

		if (!ok)
			break;
	}

	return 0;
}

void smp_cpus_up(void)
{
	int i;

	for (i = 0; i < CONFIG_NR_CPUS; i++)
		smp_cpu_up((i << 0) | (0 << 8) | (0 << 16) | (0ul << 32));

	/*
	 * here need to flush the cache since
	 * other cpu do not have enable the cache
	 * so they can not see the data
	 */
	flush_dcache_range((unsigned long)smp_holding_pen,
			CONFIG_NR_CPUS * sizeof(unsigned long));
}

void smp_init(void)
{
	int i;

	smp_holding_pen = (uint64_t *)&__smp_hoding_pen;

	for (i = 0; i < CONFIG_NR_CPUS; i++)
		get_per_cpu(cpu_id, i) = 0xffff;

	get_per_cpu(cpu_id, 0) = read_mpidr_el1() & MPIDR_ID_MASK;
}
