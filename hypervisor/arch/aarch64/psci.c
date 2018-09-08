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
#include <asm/psci.h>
#include <minos/arch.h>

static inline unsigned long psci_fn(uint32_t id, unsigned long a1,
		unsigned long a2, unsigned long a3)
{
	return smc_call(id, a1, a2, a3, 0, 0, 0);
}

unsigned long psci_cpu_on(unsigned long cpu, unsigned long entry)
{
	return psci_fn(PSCI_0_2_FN_CPU_ON, cpu, entry, 0);
}

unsigned long psci_cpu_off(unsigned long cpu)
{
	return psci_fn(PSCI_0_2_FN_CPU_OFF, cpu, 0, 0);
}
