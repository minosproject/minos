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
#include <asm/aarch64_helper.h>
#include <asm/arch.h>
#include <asm/cpu_feature.h>
#include <minos/mm.h>

#define CPU_FEATURE_BITS	128

#define CPU_FEATURE_VHE		0

static DEFINE_PER_CPU(unsigned long *, cpu_feature);

int cpu_has_feature(int feature)
{
	int ret;
	unsigned long *cf;

	if (feature >= CPU_FEATURE_BITS)
		return 0;

	cf = get_cpu_data(cpu_feature);
	ret = test_bit(feature, cf);
	put_cpu_data(cpu_feature);

	return ret;
}

int cpu_has_vhe(void)
{
	static int vhe_enable = -1;

	if (vhe_enable == -1) {
		vhe_enable = cpu_has_feature(CPU_FEATURE_VHE);
	}

	return vhe_enable;
}

static int arch_cpu_feature_init(void)
{
	unsigned long *cf;
	uint64_t value;

	cf = malloc(BITS_TO_LONGS(CPU_FEATURE_BITS));
	if (!cf)
		panic("can not allocate memory for cpu feature\n");

	memset(cf, 0, BITMAP_SIZE(CPU_FEATURE_BITS));
	get_cpu_var(cpu_feature) = cf;

	value = read_mpidr_el1();
	if (value & MPIDR_EL1_MT)
		set_bit(ARM_FEATURE_MPIDR_SHIFT, cf);

	return 0;
}
arch_initcall_percpu(arch_cpu_feature_init);
