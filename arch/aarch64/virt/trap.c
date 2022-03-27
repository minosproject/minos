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
#include <virt/vm.h>
#include <asm/trap.h>
#include <minos/minos.h>
#include <minos/smp.h>
#include <asm/reg.h>
#include <minos/sched.h>
#include <minos/irq.h>
#include <asm/svccc.h>
#include <virt/vdev.h>

static inline int taken_from_el1(uint64_t spsr)
{
	return ((spsr & 0xf) != AARCH64_SPSR_EL0t);
}

static inline void inject_virtual_data_abort(uint32_t esr_value)
{
	uint64_t hcr_el2 = read_sysreg(HCR_EL2) | HCR_EL2_VSE;

	write_sysreg(hcr_el2, HCR_EL2);
	write_sysreg(esr_value, ESR_EL1);
	wmb();
}

static int unknown_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	return 0;
}

static int wfi_wfe_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	vcpu_idle(get_current_vcpu());
	return 0;
}

static int mcr_mrc_cp15_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	switch (esr_value & HSR_CP32_REGS_MASK) {
	case HSR_CPREG32(ACTLR):
		break;
	default:
		pr_notice("mcr_mrc_cp15_handler 0x%x\n", esr_value);
		break;
	}

	return 0;
}

static int mcrr_mrrc_cp15_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	struct esr_cp64 *sysreg = (struct esr_cp64 *)&esr_value;
	unsigned long reg_value0, reg_value1;
	unsigned long reg_value;
	struct vcpu *vcpu = get_current_vcpu();
	struct arm_virt_data *arm_data = vcpu->vm->arch_data;

	switch (esr_value & HSR_CP64_REGS_MASK) {
	case HSR_CPREG64(CNTP_CVAL):
		break;

	/* for aarch32 vm and using gicv3 */
	case HSR_CPREG64(ICC_SGI1R):
	case HSR_CPREG64(ICC_ASGI1R):
	case HSR_CPREG64(ICC_SGI0R):
		if (!sysreg->read && arm_data->sgi1r_el1_trap) {
			reg_value0 = get_reg_value(reg, sysreg->reg1);
			reg_value1 = get_reg_value(reg, sysreg->reg2);
			reg_value = (reg_value1 << 32) | reg_value0;
			arm_data->sgi1r_el1_trap(vcpu, reg_value);
		}
		break;
	}

	return 0;
}

static int mcr_mrc_cp14_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	return 0;
}

static int ldc_stc_cp14_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	return 0;
}

static int access_simd_reg_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	return 0;
}

static int mcr_mrc_cp10_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	return 0;
}

static int mrrc_cp14_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	return 0;
}

static int illegal_exe_state_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	return 0;
}

static int __arm_svc_handler(gp_regs *reg, int smc)
{
	uint32_t id;
	unsigned long args[6];

	id = reg->x0;
	args[0] = reg->x1;
	args[1] = reg->x2;
	args[2] = reg->x3;
	args[3] = reg->x4;
	args[4] = reg->x5;
	args[5] = reg->x6;

	if (!(id & SVC_CTYPE_MASK))
		local_irq_enable();

	return do_svc_handler(reg, id, args, smc);
}

static int access_system_reg_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	struct esr_sysreg *sysreg = (struct esr_sysreg *)&esr_value;
	struct vcpu *vcpu = get_current_vcpu();
	struct arm_virt_data *arm_data = vcpu->vm->arch_data;
	uint32_t regindex = sysreg->reg;
	unsigned long reg_value = 0;
	int ret = 0, reg_name;

	reg_name = esr_value & ESR_SYSREG_REGS_MASK;
	reg_value = sysreg->read ? 0 : get_reg_value(reg, regindex);

	switch (reg_name) {
	case ESR_SYSREG_ICC_SGI1R_EL1:
	case ESR_SYSREG_ICC_ASGI1R_EL1:
	case ESR_SYSREG_ICC_SGI0R_EL1:
		pr_debug("access system reg SGI1R_EL1\n");
		if (!sysreg->read && (arm_data->sgi1r_el1_trap))
			arm_data->sgi1r_el1_trap(vcpu, reg_value);
		break;
	case ESR_SYSREG_CNTPCT_EL0:
	case ESR_SYSREG_CNTP_TVAL_EL0:
	case ESR_SYSREG_CNTP_CTL_EL0:
	case ESR_SYSREG_CNTP_CVAL_EL0:
		if (arm_data->phy_timer_trap) {
			ret = arm_data->phy_timer_trap(vcpu, reg_name,
					sysreg->read, &reg_value);
		}
		break;

	case ESR_SYSREG_DCZVA:
		if ((arm_data->dczva_trap) && !sysreg->read)
			ret = arm_data->dczva_trap(vcpu, reg_value);
		break;
	default:
		pr_debug("unsupport register access 0x%x %s\n",
				reg_name, sysreg->read ? "read" : "write");
		if (arm_data->sysreg_emulation) {
			ret = arm_data->sysreg_emulation(vcpu,
					reg_name, sysreg->read, &reg_value);
		}
		break;
	}

	if (sysreg->read)
		set_reg_value(reg, regindex, reg_value);

	return ret;
}

static int insabort_tfl_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	panic("%s\n", __func__);
	return 0;
}

static int misaligned_pc_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	panic("%s\n", __func__);
	return 0;
}

static inline bool dabt_isextabt(uint32_t dfsc)
{
	switch (dfsc) {
	case FSC_SEA:
	case FSC_SEA_TTW0:
	case FSC_SEA_TTW1:
	case FSC_SEA_TTW2:
	case FSC_SEA_TTW3:
	case FSC_SECC:
	case FSC_SECC_TTW0:
	case FSC_SECC_TTW1:
	case FSC_SECC_TTW2:
	case FSC_SECC_TTW3:
		return true;
	default:
		return false;
	}
}

static inline unsigned long get_faulting_ipa(unsigned long vaddr)
{
        uint64_t hpfar = read_sysreg(HPFAR_EL2);
        unsigned long ipa;

        ipa = (hpfar & HPFAR_MASK) << (12 - 4);
        ipa |= vaddr & (~(~PAGE_MASK));

        return ipa;
}

static inline bool dabt_iswrite(uint32_t esr_value)
{
	return (!!(esr_value & ESR_ELx_WNR)) ||
			(!!(esr_value & ESR_ELx_S1PTW));
}

static int dataabort_tfl_handler(gp_regs *regs, int ec, uint32_t esr_value)
{
	uint32_t dfsc = esr_value & ESR_ELx_FSC_TYPE;
	unsigned long vaddr, ipa, value;
	int ret, iswrite, reg;

	if (dabt_isextabt(dfsc)) {
		pr_err("data abort is external abort\n");
		goto out_fail;
	}

	if ((dfsc != FSC_FAULT) && (dfsc != FSC_PERM)) {
		pr_err("Unsupported data abort FSC: EC=%x xFSC=%x ESR_EL2=%x\n",
				ec, dfsc, esr_value);
		goto out_fail;
	}

	if (!(esr_value & ESR_ELx_ISV)) {
		pr_err("Instruction syndrome not valid\n");
		goto out_fail;
	}

	iswrite = dabt_iswrite(esr_value);
	reg = ESR_ELx_SRT(esr_value);
	value = iswrite ? get_reg_value(regs, reg) : 0;
	vaddr = read_sysreg(FAR_EL2);
	if ((esr_value &ESR_ELx_S1PTW) || (dfsc == FSC_FAULT))
		ipa = get_faulting_ipa(vaddr);
	else
		ipa = guest_va_to_ipa(vaddr, 1);

	ret = vdev_mmio_emulation(regs, iswrite, ipa, &value);
	if (ret) {
		pr_warn("handle mmio read/write fail 0x%x vmid:%d\n", ipa,
				get_vmid(get_current_vcpu()));
		/*
		 * if failed to handle the mmio trap inject a
		 * sync error to guest vm to generate a fault
		 */
		goto out_fail;
	}
	
	if (!iswrite)
		set_reg_value(regs, reg, value);

	return 0;

out_fail:
	inject_virtual_data_abort(esr_value);
	return -EFAULT;
}

static int stack_misalign_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	panic("%s\n", __func__);
	return 0;
}

static int floating_aarch32_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	panic("%s\n", __func__);
	return 0;
}

static int floating_aarch64_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	panic("%s\n", __func__);
	return 0;
}

static int serror_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	panic("%s\n", __func__);
	return 0;
}

int aarch64_hypercall_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	struct vcpu *vcpu = get_current_vcpu();
	struct arm_virt_data *arm_data = vcpu->vm->arch_data;

	if (arm_data->hvc_handler)
		return arm_data->hvc_handler(vcpu, reg, read_esr_el2());
	else
		return __arm_svc_handler(reg, 0);
}

int aarch64_smccall_handler(gp_regs *reg, int ec, uint32_t esr_value)
{
	struct vcpu *vcpu = get_current_vcpu();
	struct arm_virt_data *arm_data = vcpu->vm->arch_data;

	if (arm_data->hvc_handler)
		return arm_data->smc_handler(vcpu, reg, read_esr_el2());
	else
		return __arm_svc_handler(reg, 1);
}


/* type defination is at armv8-spec 1906 */
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_WFx, EC_TYPE_BOTH, wfi_wfe_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_UNKNOWN, EC_TYPE_BOTH, unknown_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_CP15_32, EC_TYPE_BOTH, mcr_mrc_cp15_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_CP15_64, EC_TYPE_AARCH32, mcrr_mrrc_cp15_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_CP14_MR, EC_TYPE_AARCH32, mcr_mrc_cp14_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_CP14_LS, EC_TYPE_AARCH32, ldc_stc_cp14_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_FP_ASIMD, EC_TYPE_BOTH, access_simd_reg_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_CP10_ID, EC_TYPE_AARCH32, mcr_mrc_cp10_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_CP14_64, EC_TYPE_AARCH32, mrrc_cp14_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_ILL, EC_TYPE_BOTH, illegal_exe_state_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_SYS64, EC_TYPE_AARCH64, access_system_reg_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_IABT_LOW, EC_TYPE_BOTH, insabort_tfl_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_PC_ALIGN, EC_TYPE_BOTH, misaligned_pc_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_DABT_LOW, EC_TYPE_BOTH, dataabort_tfl_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_SP_ALIGN, EC_TYPE_BOTH, stack_misalign_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_FP_EXC32, EC_TYPE_AARCH32, floating_aarch32_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_FP_EXC64, EC_TYPE_AARCH64, floating_aarch64_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_SERROR, EC_TYPE_BOTH, serror_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_HVC32, EC_TYPE_AARCH32, aarch64_hypercall_handler, 1, 0);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_HVC64, EC_TYPE_AARCH64, aarch64_hypercall_handler, 1, 0);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_SMC32, EC_TYPE_AARCH32, aarch64_smccall_handler, 1, 4);
DEFINE_SYNC_DESC(guest_ESR_ELx_EC_SMC64, EC_TYPE_AARCH64, aarch64_smccall_handler, 1, 4);

static struct sync_desc *guest_sync_descs[] = {
	[0 ... ESR_ELx_EC_MAX]	= &sync_desc_guest_ESR_ELx_EC_UNKNOWN,
	[ESR_ELx_EC_WFx]	= &sync_desc_guest_ESR_ELx_EC_WFx,
	[ESR_ELx_EC_CP15_32]    = &sync_desc_guest_ESR_ELx_EC_CP15_32,
	[ESR_ELx_EC_CP15_64]    = &sync_desc_guest_ESR_ELx_EC_CP15_64,
	[ESR_ELx_EC_CP14_MR]    = &sync_desc_guest_ESR_ELx_EC_CP14_MR,
	[ESR_ELx_EC_CP14_LS]    = &sync_desc_guest_ESR_ELx_EC_CP14_LS,
	[ESR_ELx_EC_FP_ASIMD]   = &sync_desc_guest_ESR_ELx_EC_FP_ASIMD,
	[ESR_ELx_EC_CP10_ID]    = &sync_desc_guest_ESR_ELx_EC_CP10_ID,
	[ESR_ELx_EC_CP14_64]	= &sync_desc_guest_ESR_ELx_EC_CP14_64,
	[ESR_ELx_EC_ILL]	= &sync_desc_guest_ESR_ELx_EC_ILL,
	[ESR_ELx_EC_SYS64]	= &sync_desc_guest_ESR_ELx_EC_SYS64,
	[ESR_ELx_EC_IABT_LOW]   = &sync_desc_guest_ESR_ELx_EC_IABT_LOW,
	[ESR_ELx_EC_PC_ALIGN]   = &sync_desc_guest_ESR_ELx_EC_PC_ALIGN,
	[ESR_ELx_EC_DABT_LOW]   = &sync_desc_guest_ESR_ELx_EC_DABT_LOW,
	[ESR_ELx_EC_SP_ALIGN]   = &sync_desc_guest_ESR_ELx_EC_SP_ALIGN,
	[ESR_ELx_EC_FP_EXC32]   = &sync_desc_guest_ESR_ELx_EC_FP_EXC32,
	[ESR_ELx_EC_FP_EXC64]   = &sync_desc_guest_ESR_ELx_EC_FP_EXC64,
	[ESR_ELx_EC_SERROR]	= &sync_desc_guest_ESR_ELx_EC_SERROR,
	[ESR_ELx_EC_HVC32]	= &sync_desc_guest_ESR_ELx_EC_HVC32,
	[ESR_ELx_EC_HVC64]	= &sync_desc_guest_ESR_ELx_EC_HVC64,
	[ESR_ELx_EC_SMC32]	= &sync_desc_guest_ESR_ELx_EC_SMC32,
	[ESR_ELx_EC_SMC64]	= &sync_desc_guest_ESR_ELx_EC_SMC64,
};

void handle_vcpu_sync_exception(gp_regs *regs)
{
	int cpuid = smp_processor_id();
	uint32_t esr_value;
	int ec_type;
	struct sync_desc *ec;
	struct vcpu *vcpu = get_current_vcpu();

	if ((!vcpu) || (vcpu->task->affinity != cpuid))
		panic("this vcpu is not belong to the pcpu");

	esr_value = read_esr_el2();
	ec_type = (esr_value & ESR_ELx_EC_MASK) >> ESR_ELx_EC_SHIFT;

	if (ec_type >= ESR_ELx_EC_MAX) {
		pr_err("unknown sync exception type from guest %d\n", ec_type);
		goto out;
	}

	pr_debug("sync from lower EL, handle 0x%x\n", ec_type);
	ec = guest_sync_descs[ec_type];
	if (ec->irq_safe)
		local_irq_enable();

	regs->pc += ec->ret_addr_adjust;
	ec->handler(regs, ec_type, esr_value);
out:
	local_irq_disable();
}
