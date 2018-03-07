#include <asm/aarch64_helper.h>
#include <mvisor/vcpu.h>
#include <asm/exception.h>
#include <mvisor/mvisor.h>
#include <mvisor/smp.h>

static unsigned long ec_unknown_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_wfi_wfe_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_mcr_mrc_cp15_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_mcrr_mrrc_cp15_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_mcr_mrc_cp14_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_ldc_stc_cp14_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_access_simd_reg_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_mcr_mrc_cp10_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_mrrc_cp14_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_illegal_exe_state_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_svc_aarch32_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_hvc_aarch32_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_smc_aarch32_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_svc_aarch64_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_hvc_aarch64_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_smc_aarch64_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_acess_system_reg_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_insabort_tfl_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_insabort_twe_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_misaligned_pc_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_dataabort_tfl_handler(uint32_t iss, uint32_t il, void *arg)
{
	uint32_t isv, sas, sse, srt, ea, cm, s1ptw, wnr, dfsc;

	isv = (iss | (1 << 24)) >> 24;
	if (!isv) {
		pr_error("Get invaild instrction syndrome %s\n", __func__);
		panic(NULL);
	}

	sas = (iss & (3 << 22)) >> 22;
	sse = (iss & (1 << 21)) >> 21;
	srt = (iss & (0xf << 16)) >> 16;
	ea = (iss & (1 << 9)) >> 9;
	cm = (iss & (1 << 8)) >> 8;
	s1ptw = (iss & (1 << 7)) >> 7;
	wnr = (iss & (1 << 6)) >> 6;
	dfsc = (iss & 0x1f);

	pr_info("sas:%d sse:%d srt:%d ea:%d cm:%d s1ptw:%d wnr:%d dfsc:%d\n",
			sas, sse, srt, ea, cm, s1ptw, wnr, dfsc);
	return 0;

}

static unsigned long ec_dataabort_twe_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_stack_misalign_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_floating_aarch32_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_floating_aarch64_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_serror_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_breakpoint_tfl_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_breakpoint_twe_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_software_step_tfl_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_software_step_twe_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_watchpoint_tfl_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_watchpoint_twe_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_bkpt_ins_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_vctor_catch_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static unsigned long ec_brk_ins_handler(uint32_t iss, uint32_t il, void *arg)
{

}

static ec_config_t ec_config_table[] = {
	{EC_UNKNOWN, EC_TYPE_BOTH, ec_unknown_handler, 4},
	{EC_WFI_WFE, EC_TYPE_BOTH, ec_wfi_wfe_handler, 4},
	{EC_MCR_MRC_CP15, EC_TYPE_BOTH, ec_mcr_mrc_cp15_handler, 4},
	{EC_MCRR_MRRC_CP15, EC_TYPE_AARCH32, ec_mcrr_mrrc_cp15_handler, 4},
	{EC_MCR_MRC_CP14, EC_TYPE_AARCH32, ec_mcr_mrc_cp14_handler, 4},
	{EC_LDC_STC_CP14, EC_TYPE_AARCH32, ec_ldc_stc_cp14_handler, 4},
	{EC_ACCESS_SIMD_REG, EC_TYPE_BOTH, ec_access_simd_reg_handler, 4},
	{EC_MCR_MRC_CP10, EC_TYPE_AARCH32, ec_mcr_mrc_cp10_handler, 4},
	{EC_MRRC_CP14, EC_TYPE_AARCH32, ec_mrrc_cp14_handler, 4},
	{EC_ILLEGAL_EXE_STATE, EC_TYPE_BOTH, ec_illegal_exe_state_handler, 4},
	{EC_SVC_AARCH32, EC_TYPE_AARCH32, ec_svc_aarch32_handler, 0},
	{EC_HVC_AARCH32, EC_TYPE_AARCH32, ec_hvc_aarch32_handler, 0},
	{EC_SMC_AARCH32, EC_TYPE_AARCH32, ec_smc_aarch32_handler, 0},
	{EC_SVC_AARCH64, EC_TYPE_AARCH64, ec_svc_aarch64_handler, 0},
	{EC_HVC_AARCH64, EC_TYPE_AARCH64, ec_hvc_aarch64_handler, 0},
	{EC_SMC_AARCH64, EC_TYPE_AARCH64, ec_smc_aarch64_handler, 0},
	{EC_ACESS_SYSTEM_REG, EC_TYPE_AARCH64, ec_acess_system_reg_handler, 4},
	{EC_INSABORT_TFL, EC_TYPE_BOTH, ec_insabort_tfl_handler, 4},
	{EC_INSABORT_TWE, EC_TYPE_BOTH, ec_insabort_twe_handler, 4},
	{EC_MISALIGNED_PC, EC_TYPE_BOTH, ec_misaligned_pc_handler, 4},
	{EC_DATAABORT_TFL, EC_TYPE_BOTH, ec_dataabort_tfl_handler, 4},
	{EC_DATAABORT_TWE, EC_TYPE_BOTH, ec_dataabort_twe_handler, 4},
	{EC_STACK_MISALIGN, EC_TYPE_BOTH, ec_stack_misalign_handler, 4},
	{EC_FLOATING_AARCH32, EC_TYPE_AARCH32, ec_floating_aarch32_handler, 4},
	{EC_FLOATING_AARCH64, EC_TYPE_AARCH64, ec_floating_aarch64_handler, 4},
	{EC_SERROR, EC_TYPE_BOTH, ec_serror_handler, 4},
	{EC_BREAKPOINT_TFL, EC_TYPE_BOTH, ec_breakpoint_tfl_handler, 4},
	{EC_BREAKPOINT_TWE, EC_TYPE_BOTH, ec_breakpoint_twe_handler, 4},
	{EC_SOFTWARE_STEP_TFL, EC_TYPE_BOTH, ec_software_step_tfl_handler, 4},
	{EC_SOFTWARE_STEP_TWE, EC_TYPE_BOTH, ec_software_step_twe_handler, 4},
	{EC_WATCHPOINT_TFL, EC_TYPE_BOTH, ec_watchpoint_tfl_handler, 4},
	{EC_WATCHPOINT_TWE, EC_TYPE_BOTH, ec_watchpoint_twe_handler, 4},
	{EC_BKPT_INS, EC_TYPE_AARCH32, ec_bkpt_ins_handler, 4},
	{EC_VCTOR_CATCH, EC_TYPE_AARCH32, ec_vctor_catch_handler, 4},
	{EC_BRK_INS, EC_TYPE_AARCH64, ec_brk_ins_handler, 4},
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

uint64_t sync_el2_handler(vcpu_t *vcpu)
{
	int cpuid = get_cpu_id();
	uint32_t esr_value;
	uint32_t ec_type, il, iss;
	ec_config_t *ec;
	long ret;

	if (get_pcpu_id(vcpu) != cpuid)
		panic("this vcpu is not belont to the pcpu");

	esr_value = read_esr_el2();
	ec_type = (esr_value & 0xfc000000) >> 26;
	il = (esr_value & 0x02000000) >> 25;
	iss = (esr_value & 0x01ffffff);
	pr_debug("value of esr_el2 ec:0x%x il:0x%x iss:0x%x\n", ec_type, il, iss);

	ec = get_ec_config(ec_type);
	if (ec == NULL) {
		ret = -EINVAL;
		goto out;
	}

	ret = ec->handler(iss, il, (void *)vcpu);

out:
	//vcpu->context.elr_el2 += ec->ret_addr_adjust;
	return 0;
}

