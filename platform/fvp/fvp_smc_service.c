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
#include <asm/svccc.h>
#include <minos/sched.h>
#include <asm/psci.h>
#include <minos/virt.h>

static int fvp_std_smc_handler(gp_regs *c,
		uint32_t id, uint64_t *args)
{
	int ret;

	switch (id) {
	case PSCI_0_2_FN_PSCI_VERSION:
		SVC_RET1(c, 0, PSCI_VERSION(0, 2));
		break;

	case PSCI_0_2_FN64_CPU_ON:
	case PSCI_0_2_FN_CPU_ON:
		ret = vcpu_power_on(current_vcpu, (int)args[0],
					(unsigned long)args[1],
					(unsigned long)args[2]);
		if (ret)
			SVC_RET1(c, ret, PSCI_RET_INVALID_PARAMS);
		break;

	case PSCI_0_2_FN_CPU_OFF:
		break;
	case PSCI_0_2_FN64_CPU_SUSPEND:
		/*
		 * only can be called by vcpu self
		 */
		ret = vcpu_suspend(c, (uint32_t)args[0], (unsigned long)args[1]);
		if (ret)
			SVC_RET1(c, 0, PSCI_RET_DENIED);
		break;

	case PSCI_0_2_FN_MIGRATE_INFO_TYPE:
		SVC_RET1(c, 0, PSCI_0_2_TOS_MP);
		break;
	default:
		break;
	}

	SVC_RET1(c, 0, PSCI_RET_SUCCESS);
}

DEFINE_SMC_HANDLER("std_smc_desc", SVC_STYPE_STDSMC,
		SVC_STYPE_STDSMC, fvp_std_smc_handler);
