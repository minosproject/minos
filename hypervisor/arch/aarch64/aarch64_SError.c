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

#include <asm/aarch64_helper.h>
#include <minos/vcpu.h>
#include <asm/exception.h>
#include <minos/minos.h>
#include <minos/smp.h>
#include <asm/processer.h>
#include <minos/sched.h>
#include <asm/vgic.h>
#include <minos/irq.h>
#include <asm/svccc.h>
#include <asm/vtimer.h>
#include <minos/virt.h>
#include <minos/vdev.h>

extern unsigned char __serror_desc_start;
extern unsigned char __serror_desc_end;

static struct serror_desc *serror_descs[MAX_SERROR_TYPE] __align_cache_line;

static int unknown_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int wfi_wfe_handler(gp_regs *reg, uint32_t esr_value)
{
	vcpu_idle();

	return 0;
}

static int mcr_mrc_cp15_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int mcrr_mrrc_cp15_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int mcr_mrc_cp14_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int ldc_stc_cp14_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int access_simd_reg_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int mcr_mrc_cp10_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int mrrc_cp14_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int illegal_exe_state_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int armsvc_aarch32_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int hvc_aarch32_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int smc_aarch32_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int armsvc_aarch64_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static inline int svc_aarch64_handler(gp_regs *reg, uint32_t esr_value, int smc)
{
	int fast;
	uint32_t id;
	uint16_t imm;
	uint64_t args[6];
	struct vcpu *vcpu = current_vcpu;

	if ((!vcpu->vm) || (get_vmid(vcpu) != 0))
		HVC_RET1(reg, -EPERM);

	imm = esr_value & 0xff;
	if (imm != 0)
		SVC_RET1(reg, -EINVAL);

	id = reg->x0;
	fast = !!(id & SVC_CTYPE_MASK);
	memcpy((void *)args, (void *)&reg->x1, 6 * sizeof(uint64_t));

	if (!fast)
		local_irq_enable();

	return do_svc_handler(reg, id, args, smc);
}

static int hvc_aarch64_handler(gp_regs *reg, uint32_t esr_value)
{
	return svc_aarch64_handler(reg, esr_value, 0);
}

static int smc_aarch64_handler(gp_regs *reg, uint32_t esr_value)
{
	return svc_aarch64_handler(reg, esr_value, 1);
}

static int access_system_reg_handler(gp_regs *reg, uint32_t esr_value)
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
			vgic_send_sgi(current_vcpu, reg_value);
		}
		break;

	case ESR_SYSREG_ICC_SGI0R_EL1:
		pr_debug("access system reg SGI0R_EL1\n");
		break;

	case ESR_SYSREG_CNTPCT_EL0:
	case ESR_SYSREG_CNTP_TVAL_EL0:
	case ESR_SYSREG_CNTP_CTL_EL0:
	case ESR_SYSREG_CNTP_CVAL_EL0:
		return vtimer_sysreg_simulation(reg, esr_value);
	}

	return ret;
}

static int insabort_tfl_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int insabort_twe_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int misaligned_pc_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static inline unsigned long get_faulting_ipa(unsigned long vaddr)
{
	uint64_t hpfar = read_sysreg(HPFAR_EL2);
	unsigned long ipa;

	ipa = (hpfar & HPFAR_MASK) << (12 - 4);
	ipa |= vaddr & (~(~PAGE_MASK));

	return ipa;
}

static int dataabort_tfl_handler(gp_regs *regs, uint32_t esr_value)
{
	int ret;
	unsigned long vaddr;
	unsigned long paddr;
	unsigned long value;
	struct esr_dabt *dabt = (struct esr_dabt *)&esr_value;

	vaddr = read_sysreg(FAR_EL2);
	paddr = get_faulting_ipa(vaddr);
	//pr_info("fault address is %x %x\n", vaddr, paddr);

	/*
	 * dfsc contain the fault type of the dataabort
	 * now only handle translation fault
	 */
	switch (dabt->dfsc & ~FSC_LL_MASK) {
	case FSC_FLT_PERM:
	case FSC_FLT_ACCESS:
	case FSC_FLT_TRANS:
		if (dabt->write)
			value = get_reg_value(regs, dabt->reg);

		ret = vdev_mmio_emulation(regs, dabt->write, paddr, &value);
		if (ret) {
			pr_warn("handle mmio read/write fail 0x%x vmid:%d\n",
					paddr, get_vmid(current_vcpu));
		} else {
			if (!dabt->write)
				set_reg_value(regs, dabt->reg, value);
		}
		break;
	default:
		pr_info("unsupport data abort type this time %d\n",
				dabt->dfsc & ~FSC_LL_MASK);
		break;
	}

	return 0;
}

static int dataabort_twe_handler(gp_regs *reg, uint32_t esr_value)
{

	pr_fatal("Unable to handle NULL pointer at address:0x%p\n",
			read_sysreg(FAR_EL2));

	return 0;
}

static int stack_misalign_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int floating_aarch32_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int floating_aarch64_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int serror_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int breakpoint_tfl_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int breakpoint_twe_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int software_step_tfl_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int software_step_twe_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int watchpoint_tfl_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int watchpoint_twe_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int bkpt_ins_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int vctor_catch_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

static int brk_ins_handler(gp_regs *reg, uint32_t esr_value)
{
	return 0;
}

/* type defination is at armv8-spec 1906 */
DEFINE_SERROR_DESC(EC_UNKNOWN, EC_TYPE_BOTH,
		unknown_handler, 1, 4);

DEFINE_SERROR_DESC(EC_WFI_WFE, EC_TYPE_BOTH,
		wfi_wfe_handler, 1, 4);

DEFINE_SERROR_DESC(EC_MCR_MRC_CP15, EC_TYPE_BOTH,
		mcr_mrc_cp15_handler, 1, 4);

DEFINE_SERROR_DESC(EC_MCRR_MRRC_CP15, EC_TYPE_AARCH32,
		mcrr_mrrc_cp15_handler, 1, 4);

DEFINE_SERROR_DESC(EC_MCR_MRC_CP14, EC_TYPE_AARCH32,
		mcr_mrc_cp14_handler, 1, 4);

DEFINE_SERROR_DESC(EC_LDC_STC_CP14, EC_TYPE_AARCH32,
		ldc_stc_cp14_handler, 1, 4);

DEFINE_SERROR_DESC(EC_ACCESS_SIMD_REG, EC_TYPE_BOTH,
		access_simd_reg_handler, 1, 4);

DEFINE_SERROR_DESC(EC_MCR_MRC_CP10, EC_TYPE_AARCH32,
		mcr_mrc_cp10_handler, 1, 4);

DEFINE_SERROR_DESC(EC_MRRC_CP14, EC_TYPE_AARCH32,
		mrrc_cp14_handler, 1, 4);

DEFINE_SERROR_DESC(EC_ILLEGAL_EXE_STATE, EC_TYPE_BOTH,
		illegal_exe_state_handler, 1, 4);

DEFINE_SERROR_DESC(EC_SVC_AARCH32, EC_TYPE_AARCH32,
		armsvc_aarch32_handler, 0, 0);

DEFINE_SERROR_DESC(EC_HVC_AARCH32, EC_TYPE_AARCH32,
		hvc_aarch32_handler, 0, 0);

DEFINE_SERROR_DESC(EC_SMC_AARCH32, EC_TYPE_AARCH32,
		smc_aarch32_handler, 0, 0);

DEFINE_SERROR_DESC(EC_SVC_AARCH64, EC_TYPE_AARCH64,
		armsvc_aarch64_handler, 0, 0);

DEFINE_SERROR_DESC(EC_HVC_AARCH64, EC_TYPE_AARCH64,
		hvc_aarch64_handler, 0, 0);

DEFINE_SERROR_DESC(EC_SMC_AARCH64, EC_TYPE_AARCH64,
		smc_aarch64_handler, 0, 4);

DEFINE_SERROR_DESC(EC_ACESS_SYSTEM_REG, EC_TYPE_AARCH64,
		access_system_reg_handler, 1, 4);

DEFINE_SERROR_DESC(EC_INSABORT_TFL, EC_TYPE_BOTH,
		insabort_tfl_handler, 1, 4);

DEFINE_SERROR_DESC(EC_INSABORT_TWE, EC_TYPE_BOTH,
		insabort_twe_handler, 1, 4);

DEFINE_SERROR_DESC(EC_MISALIGNED_PC, EC_TYPE_BOTH,
		misaligned_pc_handler, 1, 4);

DEFINE_SERROR_DESC(EC_DATAABORT_TFL, EC_TYPE_BOTH,
		dataabort_tfl_handler, 1, 4);

DEFINE_SERROR_DESC(EC_DATAABORT_TWE, EC_TYPE_BOTH,
		dataabort_twe_handler, 1, 4);

DEFINE_SERROR_DESC(EC_STACK_MISALIGN, EC_TYPE_BOTH,
		stack_misalign_handler, 1, 4);

DEFINE_SERROR_DESC(EC_FLOATING_AARCH32, EC_TYPE_AARCH32,
		floating_aarch32_handler, 1, 4);

DEFINE_SERROR_DESC(EC_FLOATING_AARCH64, EC_TYPE_AARCH64,
		floating_aarch64_handler, 1, 4);

DEFINE_SERROR_DESC(EC_SERROR, EC_TYPE_BOTH, serror_handler, 1, 4);

DEFINE_SERROR_DESC(EC_BREAKPOINT_TFL, EC_TYPE_BOTH,
		breakpoint_tfl_handler, 1, 4);

DEFINE_SERROR_DESC(EC_BREAKPOINT_TWE, EC_TYPE_BOTH,
		breakpoint_twe_handler, 1, 4);

DEFINE_SERROR_DESC(EC_SOFTWARE_STEP_TFL, EC_TYPE_BOTH,
		software_step_tfl_handler, 1, 4);

DEFINE_SERROR_DESC(EC_SOFTWARE_STEP_TWE, EC_TYPE_BOTH,
		software_step_twe_handler, 1, 4);

DEFINE_SERROR_DESC(EC_WATCHPOINT_TFL, EC_TYPE_BOTH,
		watchpoint_tfl_handler, 1, 4);

DEFINE_SERROR_DESC(EC_WATCHPOINT_TWE, EC_TYPE_BOTH,
		watchpoint_twe_handler, 1, 4);

DEFINE_SERROR_DESC(EC_BKPT_INS, EC_TYPE_AARCH32, bkpt_ins_handler, 1, 4);

DEFINE_SERROR_DESC(EC_VCTOR_CATCH, EC_TYPE_AARCH32,
		vctor_catch_handler, 1, 4);

DEFINE_SERROR_DESC(EC_BRK_INS, EC_TYPE_AARCH64,
		brk_ins_handler, 1, 4);

void SError_from_lower_EL_handler(gp_regs *data)
{
	int cpuid = get_cpu_id();
	uint32_t esr_value;
	int ec_type;
	struct serror_desc *ec;
	struct vcpu *vcpu = current_vcpu;

	if ((!vcpu) || (vcpu->affinity != cpuid))
		panic("this vcpu is not belont to the pcpu");

	exit_from_guest(current_vcpu, data);

	esr_value = data->esr_elx;
	ec_type = (esr_value & 0xfc000000) >> 26;

	ec = serror_descs[ec_type];
	if (ec == NULL)
		goto out;

	/*
	 * how to deal with the return value
	 * TBD
	 */
	data->elr_elx += ec->ret_addr_adjust;
	ec->handler(data, esr_value);
out:
	local_irq_disable();

	enter_to_guest(current_vcpu, NULL);
}

void SError_from_current_EL_handler(gp_regs *data)
{
	uint32_t esr_value;
	uint32_t ec_type;
	struct serror_desc *ec;

	esr_value = read_esr_el2();
	ec_type = (esr_value & 0xfc000000) >> 26;
	ec = serror_descs[ec_type];
	if (ec != NULL)
		ec->handler(data, esr_value);

	dump_stack(data, NULL);
	while (1);
}

static int aarch64_serror_init(void)
{
	int size, i;
	unsigned long start, end;
	struct serror_desc *desc;

	memset((char *)serror_descs, 0, MAX_SERROR_TYPE
			* sizeof(struct serror_desc *));

	start = (unsigned long)&__serror_desc_start;
	end = (unsigned long)&__serror_desc_end;
	size = (end - start) / sizeof(struct serror_desc);
	desc = (struct serror_desc *)start;

	for (i = 0; i < size; i++) {
		serror_descs[desc->type] = desc;
		desc++;
	}

	return 0;
}

void serror_c_handler(gp_regs *regs)
{
	if (taken_from_guest(regs))
		SError_from_lower_EL_handler(regs);
	else
		SError_from_current_EL_handler(regs);
}

arch_initcall(aarch64_serror_init);
