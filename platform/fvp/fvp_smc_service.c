#include <minos/minos.h>
#include <asm/svccc.h>
#include <minos/sched.h>
#include <minos/psci.h>
#include <virt/virt.h>

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
		ret = vcpu_power_on((struct vcpu *)c, (int)args[0],
					(unsigned long)args[1],
					(unsigned long)args[2]);
		if (ret)
			SVC_RET1(c, ret, PSCI_RET_INVALID_PARAMS);
		break;

	case PSCI_0_2_FN_CPU_OFF:
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
