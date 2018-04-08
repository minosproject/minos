#include <asm/aarch64_helper.h>
#include <mvisor/vcpu.h>
#include <asm/exception.h>
#include <mvisor/mvisor.h>
#include <mvisor/smp.h>
#include <asm/processer.h>
#include <mvisor/mmio.h>
#include <mvisor/sched.h>
#include <asm/vgic.h>

static int unknown_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int wfi_wfe_handler(vcpu_regs *reg, uint32_t esr_value)
{
	vcpu_t *vcpu = (vcpu_t *)reg;

	vcpu_idle(vcpu);

	return 0;
}

static int mcr_mrc_cp15_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int mcrr_mrrc_cp15_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int mcr_mrc_cp14_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int ldc_stc_cp14_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int access_simd_reg_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int mcr_mrc_cp10_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int mrrc_cp14_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int illegal_exe_state_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int svc_aarch32_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int hvc_aarch32_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int smc_aarch32_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int svc_aarch64_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int hvc_aarch64_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int smc_aarch64_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int access_system_reg_handler(vcpu_regs *reg, uint32_t esr_value)
{
	unsigned long ret = 0;
	struct esr_sysreg *sysreg = (struct esr_sysreg *)&esr_value;
	uint32_t regindex = sysreg->reg;
	unsigned long reg_value;

	switch (esr_value & ESR_SYSREG_REGS_MASK) {
	case ESR_SYSREG_ICC_SGI1R_EL1:
	case ESR_SYSREG_ICC_ASGI1R_EL1:
		pr_debug("access system reg SGI1R_EL1\n");
		if (!sysreg->read) {
			reg_value = get_reg_value(reg, regindex);
			vgic_send_sgi((vcpu_t *)reg, reg_value);
		}
		break;

	case ESR_SYSREG_ICC_SGI0R_EL1:
		pr_debug("access system reg SGI0R_EL1\n");
		break;
	}

	return ret;
}

static int insabort_tfl_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int insabort_twe_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int misaligned_pc_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int get_faulting_ipa(unsigned long vaddr)
{
	uint64_t hpfar = read_sysreg(HPFAR_EL2);
	unsigned long ipa;

	ipa = (hpfar & HPFAR_MASK) << (12 - 4);

	/*
	 * fix the page size to 64k, this will be modified
	 * TBD
	 */
	//ipa |= vaddr & (~PAGE_MASK);
	ipa |= vaddr & (~(~(64 * 1024 -1)));

	return ipa;
}

static int dataabort_tfl_handler(vcpu_regs *regs, uint32_t esr_value)
{
	struct esr_dabt *dabt = (struct esr_dabt *)&esr_value;
	unsigned long vaddr;
	unsigned long paddr;
	int ret;
	unsigned long value;

	vaddr = read_sysreg(FAR_EL2);
	paddr = get_faulting_ipa(vaddr);
	pr_debug("fault address is %x %x\n", vaddr, paddr);

	/*
	 * dfsc contain the fault type of the dataabort
	 * now only handle translation fault
	 */
	switch (dabt->dfsc & ~FSC_LL_MASK) {
	case FSC_FLT_TRANS:
		if (dabt->write)
			value = get_reg_value(regs, dabt->reg);

		ret = do_mmio_emulation(regs, dabt->write, paddr, &value);
		if (!ret) {
			pr_warning("handle mmio read/write fail\n");
		} else {
			if (!dabt->write)
				set_reg_value(regs, dabt->reg, value);
		}
		break;
	default:
		panic("unsupport data abort type this time\n");
	}

	return 0;
}

static int dataabort_twe_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int stack_misalign_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int floating_aarch32_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int floating_aarch64_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int serror_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int breakpoint_tfl_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int breakpoint_twe_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int software_step_tfl_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int software_step_twe_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int watchpoint_tfl_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int watchpoint_twe_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int bkpt_ins_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int vctor_catch_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static int brk_ins_handler(vcpu_regs *reg, uint32_t esr_value)
{

}

static ec_config_t ec_config_table[] = {
	{EC_UNKNOWN, EC_TYPE_BOTH, unknown_handler, 4},
	{EC_WFI_WFE, EC_TYPE_BOTH, wfi_wfe_handler, 4},
	{EC_MCR_MRC_CP15, EC_TYPE_BOTH, mcr_mrc_cp15_handler, 4},
	{EC_MCRR_MRRC_CP15, EC_TYPE_AARCH32, mcrr_mrrc_cp15_handler, 4},
	{EC_MCR_MRC_CP14, EC_TYPE_AARCH32, mcr_mrc_cp14_handler, 4},
	{EC_LDC_STC_CP14, EC_TYPE_AARCH32, ldc_stc_cp14_handler, 4},
	{EC_ACCESS_SIMD_REG, EC_TYPE_BOTH, access_simd_reg_handler, 4},
	{EC_MCR_MRC_CP10, EC_TYPE_AARCH32, mcr_mrc_cp10_handler, 4},
	{EC_MRRC_CP14, EC_TYPE_AARCH32, mrrc_cp14_handler, 4},
	{EC_ILLEGAL_EXE_STATE, EC_TYPE_BOTH, illegal_exe_state_handler, 4},
	{EC_SVC_AARCH32, EC_TYPE_AARCH32, svc_aarch32_handler, 0},
	{EC_HVC_AARCH32, EC_TYPE_AARCH32, hvc_aarch32_handler, 0},
	{EC_SMC_AARCH32, EC_TYPE_AARCH32, smc_aarch32_handler, 0},
	{EC_SVC_AARCH64, EC_TYPE_AARCH64, svc_aarch64_handler, 0},
	{EC_HVC_AARCH64, EC_TYPE_AARCH64, hvc_aarch64_handler, 0},
	{EC_SMC_AARCH64, EC_TYPE_AARCH64, smc_aarch64_handler, 0},
	{EC_ACESS_SYSTEM_REG, EC_TYPE_AARCH64, access_system_reg_handler, 4},
	{EC_INSABORT_TFL, EC_TYPE_BOTH, insabort_tfl_handler, 4},
	{EC_INSABORT_TWE, EC_TYPE_BOTH, insabort_twe_handler, 4},
	{EC_MISALIGNED_PC, EC_TYPE_BOTH, misaligned_pc_handler, 4},
	{EC_DATAABORT_TFL, EC_TYPE_BOTH, dataabort_tfl_handler, 4},
	{EC_DATAABORT_TWE, EC_TYPE_BOTH, dataabort_twe_handler, 4},
	{EC_STACK_MISALIGN, EC_TYPE_BOTH, stack_misalign_handler, 4},
	{EC_FLOATING_AARCH32, EC_TYPE_AARCH32, floating_aarch32_handler, 4},
	{EC_FLOATING_AARCH64, EC_TYPE_AARCH64, floating_aarch64_handler, 4},
	{EC_SERROR, EC_TYPE_BOTH, serror_handler, 4},
	{EC_BREAKPOINT_TFL, EC_TYPE_BOTH, breakpoint_tfl_handler, 4},
	{EC_BREAKPOINT_TWE, EC_TYPE_BOTH, breakpoint_twe_handler, 4},
	{EC_SOFTWARE_STEP_TFL, EC_TYPE_BOTH, software_step_tfl_handler, 4},
	{EC_SOFTWARE_STEP_TWE, EC_TYPE_BOTH, software_step_twe_handler, 4},
	{EC_WATCHPOINT_TFL, EC_TYPE_BOTH, watchpoint_tfl_handler, 4},
	{EC_WATCHPOINT_TWE, EC_TYPE_BOTH, watchpoint_twe_handler, 4},
	{EC_BKPT_INS, EC_TYPE_AARCH32, bkpt_ins_handler, 4},
	{EC_VCTOR_CATCH, EC_TYPE_AARCH32, vctor_catch_handler, 4},
	{EC_BRK_INS, EC_TYPE_AARCH64, brk_ins_handler, 4},
};

ec_config_t *get_ec_config(uint32_t ec_type)
{
	int i;
	ec_config_t *config;
	int size = sizeof(ec_config_table) / sizeof(ec_config_t);

	for (i = 0; i < size; i++) {
		config = &ec_config_table[i];
		if (config->type == ec_type)
			return config;
	}

	return NULL;
}

void SError_from_el1_handler(vcpu_regs *data)
{
	int cpuid = get_cpu_id();
	uint32_t esr_value;
	uint32_t ec_type;
	ec_config_t *ec;
	long ret;
	vcpu_t *vcpu = (vcpu_t *)data;

	vmm_exit_from_guest(vcpu);

	if (get_pcpu_id(vcpu) != cpuid)
		panic("this vcpu is not belont to the pcpu");

	esr_value = data->esr_el2;
	ec_type = (esr_value & 0xfc000000) >> 26;
	pr_debug("value of esr_el2 ec:0x%x il:0x%x iss:0x%x\n", ec_type);

	ec = get_ec_config(ec_type);
	if (ec == NULL) {
		ret = -EINVAL;
		return;
	}

	/*
	 * how to deal with the return value
	 * TBD
	 */
	ec->handler(data, esr_value);
	data->elr_el2 += ec->ret_addr_adjust;
}

void SError_from_el2_handler(vcpu_regs *data)
{
	while (1);
}

