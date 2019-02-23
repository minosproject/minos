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

static int std_smc_handler(gp_regs *c,
		uint32_t id, unsigned long *args)
{
	int ret;

	pr_debug("psci function id 0x%x\n", id);

	switch (id) {
	case PSCI_0_2_FN_PSCI_VERSION:
		SVC_RET1(c, PSCI_VERSION(1, 0));
		break;

	case PSCI_0_2_FN64_CPU_ON:
	case PSCI_0_2_FN_CPU_ON:
		pr_info("request vcpu on\n");
		ret = vcpu_power_on(get_current_vcpu(),
				args[0], args[1], args[2]);
		if (ret)
			SVC_RET1(c, PSCI_RET_INVALID_PARAMS);
		break;

	case PSCI_0_2_FN_CPU_OFF:
		/* virtual vcpu only support freeze mode */
		pr_info("request vcpu off\n");
		ret = vcpu_off(get_current_vcpu());
		if (ret)
			SVC_RET1(c, PSCI_RET_INTERNAL_FAILURE);
		break;
	case PSCI_0_2_FN64_CPU_SUSPEND:
		/*
		 * only can be called by vcpu self
		 */
		ret = vcpu_suspend(get_current_vcpu(), c,
				(uint32_t)args[0], args[1]);
		if (ret)
			SVC_RET1(c, PSCI_RET_DENIED);
		break;

	case PSCI_0_2_FN_MIGRATE_INFO_TYPE:
		SVC_RET1(c, PSCI_0_2_TOS_MP);
		break;

	case PSCI_0_2_FN_AFFINITY_INFO:
	case PSCI_0_2_FN64_AFFINITY_INFO:
		/* all spi irq will send to vm0 do nothing */
		SVC_RET1(c, PSCI_0_2_AFFINITY_LEVEL_OFF);
		break;

	case PSCI_0_2_FN_SYSTEM_OFF:
		/* request reset by it self */
		vm_power_off(get_vmid(get_current_vcpu()), NULL);
		break;

	case PSCI_0_2_FN_SYSTEM_RESET:
		vm_reset(get_vmid(get_current_vcpu()), NULL);
		break;

	case PSCI_1_0_FN_SYSTEM_SUSPEND:
	case PSCI_1_0_FN64_SYSTEM_SUSPEND:
		/* only can be called by vcpu0 itself */
		vm_suspend(get_vmid(get_current_vcpu()));
		break;

	case PSCI_1_0_FN_PSCI_FEATURES:
		switch (args[0]) {
		case ARM_SMCCC_VERSION_FUNC_ID:
		case PSCI_0_2_FN64_CPU_SUSPEND:
		case PSCI_1_0_FN_SYSTEM_SUSPEND:
		case PSCI_1_0_FN64_SYSTEM_SUSPEND:
			SVC_RET1(c, PSCI_RET_SUCCESS);
			break;
		default:
			SVC_RET1(c, PSCI_RET_NOT_SUPPORTED);
			break;
		}
		break;

	default:
		break;
	}

	SVC_RET1(c, PSCI_RET_SUCCESS);
}

DEFINE_SMC_HANDLER("std_smc_desc", SVC_STYPE_STDSMC,
		SVC_STYPE_STDSMC, std_smc_handler);
