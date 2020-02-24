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
#include <minos/arch.h>
#include <minos/of.h>
#include <asm/psci.h>
#include <minos/mmu.h>

extern unsigned char __smp_affinity_id;
extern phy_addr_t smp_holding_address[CONFIG_NR_CPUS];

static inline unsigned long psci_fn(uint32_t id, unsigned long a1,
		unsigned long a2, unsigned long a3)
{
	return smc_call(id, a1, a2, a3, 0, 0, 0);
}

int psci_cpu_on(unsigned long cpu, unsigned long entry)
{
	return (int)psci_fn(PSCI_0_2_FN_CPU_ON, cpu, entry, 0);
}

int psci_cpu_off(unsigned long cpu)
{
	return (int)psci_fn(PSCI_0_2_FN_CPU_OFF, cpu, 0, 0);
}

void psci_system_reboot(int mode, const char *cmd)
{
	psci_fn(PSCI_0_2_FN_SYSTEM_RESET, 0, 0, 0);
}

void psci_system_shutdown(void)
{
	psci_fn(PSCI_0_2_FN_SYSTEM_OFF, 0, 0, 0);
}

int spin_table_cpu_on(unsigned long affinity, unsigned long entry)
{
	int cpu = affinity_to_cpuid(affinity);

	if (smp_holding_address[cpu] != 0) {
		io_remap(smp_holding_address[cpu], smp_holding_address[cpu],
				sizeof(uint64_t));
		*(unsigned long *)smp_holding_address[cpu] = entry;

		/* flush the cache and send signal to other cpu */
		flush_dcache_range(smp_holding_address[cpu], sizeof(uint64_t));
		sev();
		io_unmap(smp_holding_address[cpu], sizeof(uint64_t));
		return 0;
	}

	return -EINVAL;
}
