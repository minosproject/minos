/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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

#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>
#include <virt/vm.h>
#include <virt/vmodule.h>
#include <asm/arch.h>
#include <minos/string.h>
#include <minos/print.h>
#include <minos/sched.h>
#include <minos/calltrace.h>
#include <minos/smp.h>
#include <minos/of.h>
#include <minos/platform.h>
#include <minos/task.h>
#include <virt/vm.h>
#include <virt/virq.h>
#include <virt/os.h>
#include <asm/vtcb.h>
#include <asm/tlb.h>

static uint32_t mpidr_el1[NR_CPUS];

void flush_all_tlb_mm(struct mm_struct *mm)
{
	struct vm *vm = container_of(mm, struct vm, mm);
	uint64_t vttbr = vtop(mm->pgdp) | ((uint64_t)vm->vmid << 48);
	unsigned long flags;
	uint64_t old_vttbr;

	local_irq_save(flags);

	/*
	 * switch to the target VM's VTTBR to VTTBR_EL2, make sure
	 * use the correct vmid.
	 */
	old_vttbr = read_sysreg(VTTBR_EL2);
	write_sysreg(vttbr, VTTBR_EL2);

	/*
	 * flush all the tlb with the vmid.
	 */
	flush_all_tlb_guest();

	/*
	 * restore the origin vttbr.
	 */
	write_sysreg(old_vttbr, VTTBR_EL2);

	local_irq_restore(flags);
}

void arch_set_virq_flag(void)
{
	uint64_t hcr_el2;

	hcr_el2 = read_sysreg(HCR_EL2);
	hcr_el2 |= HCR_EL2_VI;
	write_sysreg(hcr_el2, HCR_EL2);
	dsb();
}

void arch_set_vfiq_flag(void)
{
	uint64_t hcr_el2;

	hcr_el2 = read_sysreg(HCR_EL2);
	hcr_el2 |= HCR_EL2_VF;
	write_sysreg(hcr_el2, HCR_EL2);
	dsb();
}

void arch_clear_virq_flag(void)
{
	uint64_t hcr_el2;

	hcr_el2 = read_sysreg(HCR_EL2);
	hcr_el2 &= ~HCR_EL2_VI;
	hcr_el2 &= ~HCR_EL2_VF;
	write_sysreg(hcr_el2, HCR_EL2);
	dsb();
}

void arch_clear_vfiq_flag(void)
{
	uint64_t hcr_el2;

	hcr_el2 = read_sysreg(HCR_EL2);
	hcr_el2 &= ~HCR_EL2_VF;
	write_sysreg(hcr_el2, HCR_EL2);
	dsb();
}

void arch_vcpu_init(struct vcpu *vcpu, void *entry, void *arg)
{
	struct task *task = vcpu->task;
	gp_regs *regs;

	regs = stack_to_gp_regs(task->stack_top);
	memset(regs, 0, sizeof(gp_regs));
	task->stack_base = task->stack_top - sizeof(gp_regs);

	regs->pc = (uint64_t)entry;

	if (task_is_32bit(vcpu->task))
		regs->pstate = AARCH32_SVC | \
				AARCH64_SPSR_F | \
				AARCH64_SPSR_I | \
				AARCH64_SPSR_A | (1 << 4);
	else
		regs->pstate = AARCH64_SPSR_EL1h | \
				AARCH64_SPSR_F | \
				AARCH64_SPSR_I | \
				AARCH64_SPSR_A;
}

static inline uint64_t generate_vtcr_el2(void)
{
	uint64_t value = 0;

	/*
	 * vtcr_el2 used to defined the memory attribute
	 * for the EL1, this is defined by hypervisor
	 * and may do not related to physical information
	 */
	value |= (24 << 0);	// t0sz = 0x10 40bits vaddr
	value |= (0x01 << 6);	// SL0: 4kb start at level1
	value |= (0x1 << 8);	// Normal memory, Inner WBWA
	value |= (0x1 << 10);	// Normal memory, Outer WBWA
	value |= (0x3 << 12);	// Inner Shareable

	// TG0 4K
	value |= (0x0 << 14);

	// PS --- pysical size 1TB
	value |= (2 << 16);

	// vmid -- 8bit
	value |= (0x0 << 19);

	return value;
}

static inline uint64_t generate_vttbr_el2(uint32_t vmid, unsigned long base)
{
	return (base | ((uint64_t)vmid << 48));
}

void arch_vcpu_state_init(struct vcpu *vcpu, void *c)
{
	struct vcpu_context *context = (struct vcpu_context *)c;
	struct vm *vm = vcpu->vm;
	uint64_t value;

	memset(context, 0, sizeof(*context));

	/*
	 * HVC : enable hyper call function
	 * TWI : trap wfi - default enable, disable by dts
	 * TWE : trap wfe - default disable
	 * TIDCP : Trap implementation defined functionality
	 * IMP : physical irq routing
	 * FMO : physical firq routing
	 * BSU_IS : Barrier Shareability upgrade
	 * FB : force broadcast when do some tlb ins
	 * PTW : protect table walk
	 * TSC : trap smc ins
	 * TACR : Trap Auxiliary Control Registers
	 * AMO : Physical SError interrupt routing.
	 * RW : low level is 64bit, when 0 is 32 bit
	 * VM : enable virtualzation
	 */
	value = read_sysreg64(HCR_EL2);
	context->hcr_el2 = value | HCR_EL2_VM |
		     HCR_EL2_TIDCP | HCR_EL2_IMO | HCR_EL2_FMO |
		     HCR_EL2_BSU_IS | HCR_EL2_FB | HCR_EL2_PTW |
		     HCR_EL2_TSC | HCR_EL2_TACR | HCR_EL2_AMO;

	/*
	 * usually there will be so many wfis from the VM
	 * in some case this will have much infulence to
	 * the system, add this flag to disable WFI trap.
	 */
	if (!(vcpu->vm->flags & VM_FLAGS_NATIVE_WFI))
		context->hcr_el2 |= HCR_EL2_TWI;

	if (!task_is_32bit(vcpu->task))
		context->hcr_el2 |= HCR_EL2_RW;

	/*
	 * this require HVM's vcpu affinity need start with 0
	 */
	if (vm_is_host_vm(vcpu->vm))
		context->vmpidr = cpuid_to_affinity(get_vcpu_id(vcpu));
	else
		context->vmpidr = get_vcpu_id(vcpu);

	pr_notice("vmpidr is 0x%x\n", context->vmpidr);

	context->cpacr = 0x3 << 20;

	if (vm_is_native(vcpu->vm))
		context->vpidr = mpidr_el1[vcpu_affinity(vcpu)];
	else
		context->vpidr = 0x410fc050;	/* arm fvp */

	/*
	 * enable dc zva trap, the apple soc use zva size 64
	 * fixed, which may not equal to the target platform
	 * so need trap dc zva
	 */
	if (vcpu->vm->os->type == OS_TYPE_XNU)
		context->hcr_el2 |= HCR_EL2_TDZ;

	context->vtcr_el2 = generate_vtcr_el2();
	context->vttbr_el2 = generate_vttbr_el2(vm->vmid, vtop(vm->mm.pgdp));
	context->ttbr0_el1 = 0;
	context->ttbr1_el1 = 0;
	context->mair_el1 = 0;
	context->tcr_el1 = 0;
	context->par_el1 = 0;
	context->amair_el1 = 0;
}

static void arch_vcpu_state_save(struct vcpu *vcpu, void *c)
{
	struct vcpu_context *context = (struct vcpu_context *)c;

	context->vbar_el1 = read_sysreg(ARM64_VBAR_EL1);
	context->esr_el1 = read_sysreg(ARM64_ESR_EL1);
	context->elr_el1 = read_sysreg(ARM64_ELR_EL1);
	context->vmpidr = read_sysreg(ARM64_VMPIDR_EL2);
	context->vpidr = read_sysreg(ARM64_VPIDR_EL2);
	context->sctlr_el1 = read_sysreg(ARM64_SCTLR_EL1);
	context->hcr_el2 = read_sysreg(ARM64_HCR_EL2);
	context->sp_el1 = read_sysreg(ARM64_SP_EL1);
	context->sp_el0 = read_sysreg(ARM64_SP_EL0);
	context->spsr_el1 = read_sysreg(ARM64_SPSR_EL1);
	context->far_el1 = read_sysreg(ARM64_FAR_EL1);
	context->actlr_el1 = read_sysreg(ARM64_ACTLR_EL1);
	context->tpidr_el1 = read_sysreg(ARM64_TPIDR_EL1);
	context->csselr = read_sysreg(ARM64_CSSELR_EL1);
	context->cpacr = read_sysreg(ARM64_CPACR_EL1);
	context->contextidr = read_sysreg(ARM64_CONTEXTIDR_EL1);
	context->tpidr_el0 = read_sysreg(ARM64_TPIDR_EL0);
	context->tpidrro_el0 = read_sysreg(ARM64_TPIDRRO_EL0);
	context->cntkctl = read_sysreg(ARM64_CNTKCTL_EL1);
	context->afsr0 = read_sysreg(ARM64_AFSR0_EL1);
	context->afsr1 = read_sysreg(ARM64_AFSR1_EL1);

	if (task_is_32bit(vcpu->task)) {
		//context->teecr = read_sysreg32(TEECR32_EL1);
		//context->teehbr = read_sysreg32(TEEHBR32_EL1);
		context->dacr32_el2 = read_sysreg32(ARM64_DACR32_EL2);
		context->ifsr32_el2 = read_sysreg32(ARM64_IFSR32_EL2);
	}

	context->vtcr_el2 = read_sysreg(ARM64_VTCR_EL2);
	context->vttbr_el2 = read_sysreg(ARM64_VTTBR_EL2);
	context->ttbr0_el1 = read_sysreg(ARM64_TTBR0_EL1);
	context->ttbr1_el1 = read_sysreg(ARM64_TTBR1_EL1);
	context->mair_el1 = read_sysreg(ARM64_MAIR_EL1);
	context->tcr_el1 = read_sysreg(ARM64_TCR_EL1);
	context->par_el1 = read_sysreg(ARM64_PAR_EL1);
	context->amair_el1 = read_sysreg(ARM64_AMAIR_EL1);

	mb();
}

static void arch_vcpu_state_restore(struct vcpu *vcpu, void *c)
{
	struct vcpu_context *context = (struct vcpu_context *)c;

	write_sysreg(context->vbar_el1, VBAR_EL1);
	write_sysreg(context->esr_el1, ESR_EL1);
	write_sysreg(context->elr_el1, ELR_EL1);
	write_sysreg(context->vmpidr, VMPIDR_EL2);
	write_sysreg(context->vpidr, VPIDR_EL2);
	write_sysreg(context->sctlr_el1, SCTLR_EL1);
	write_sysreg(context->hcr_el2, HCR_EL2);
	write_sysreg(context->sp_el1, SP_EL1);
	write_sysreg(context->sp_el0, SP_EL0);
	write_sysreg(context->spsr_el1, SPSR_EL1);
	write_sysreg(context->far_el1, FAR_EL1);
	write_sysreg(context->actlr_el1, ACTLR_EL1);
	write_sysreg(context->tpidr_el1, TPIDR_EL1);
	write_sysreg(context->csselr, CSSELR_EL1);
	write_sysreg(context->cpacr, CPACR_EL1);
	write_sysreg(context->contextidr, CONTEXTIDR_EL1);
	write_sysreg(context->tpidr_el0, TPIDR_EL0);
	write_sysreg(context->tpidrro_el0, TPIDRRO_EL0);
	write_sysreg(context->cntkctl, CNTKCTL_EL1);
	write_sysreg(context->afsr0, AFSR0_EL1);
	write_sysreg(context->afsr1, AFSR1_EL1);

	if (task_is_32bit(vcpu->task)) {
		//write_sysreg(context->teecr, TEECR32_EL1);
		//write_sysreg(context->teehbr, TEEHBR32_EL1);
		write_sysreg(context->dacr32_el2, DACR32_EL2);
		write_sysreg(context->ifsr32_el2, IFSR32_EL2);
	}

	write_sysreg(context->vtcr_el2, ARM64_VTCR_EL2);
	write_sysreg(context->vttbr_el2, ARM64_VTTBR_EL2);
	write_sysreg(context->ttbr0_el1, ARM64_TTBR0_EL1);
	write_sysreg(context->ttbr1_el1, ARM64_TTBR1_EL1);
	write_sysreg(context->mair_el1, ARM64_MAIR_EL1);
	write_sysreg(context->amair_el1, ARM64_AMAIR_EL1);
	write_sysreg(context->tcr_el1, ARM64_TCR_EL1);
	write_sysreg(context->par_el1, ARM64_PAR_EL1);

	mb();
}

static void arch_vcpu_state_dump(struct vcpu *vcpu, void *context)
{
	struct vcpu_context *c = (struct vcpu_context *)context;

	pr_notice("----- dump vcpu context -----\n");
	pr_notice(" vbar_el1: 0x%16lx    esr_el1: 0x%16lx\n", c->vbar_el1, c->esr_el1);
	pr_notice("   sp_el1: 0x%16lx contextidr: 0x%16lx\n", c->sp_el1, c->contextidr);
	pr_notice("   sp_el0: 0x%16lx    elr_el1: 0x%16lx\n", c->sp_el0, c->elr_el1);
	pr_notice("   vmpidr: 0x%16lx  tpidr_el0: 0x%16lx\n", c->vmpidr, c->tpidr_el0);
	pr_notice("    vpidr: 0x%16lx  sctlr_el1: 0x%16lx\n", c->vpidr, c->sctlr_el1);
	pr_notice("  hcr_el2: 0x%16lx    tpidrro: 0x%16lx\n", c->hcr_el2, c->tpidrro_el0);
	pr_notice(" spsr_el1: 0x%16lx    far_el1: 0x%16lx\n", c->spsr_el1, c->far_el1);
	pr_notice("actlr_el1: 0x%16lx    cntkctl: 0x%16lx\n", c->actlr_el1, c->cntkctl);
	pr_notice("tpidr_el1: 0x%16lx     csselr: 0x%16lx\n", c->tpidr_el1, c->csselr);
	pr_notice("    cpacr: 0x%16lx      afsr0: 0x%16lx\n", c->cpacr, c->afsr0);
	pr_notice("    afsr1: 0x%16lx   vtcr_el2: 0x%16lx\n", c->afsr1, c->vtcr_el2);
	pr_notice("ttbr0_el1: 0x%16lx  ttbr1_el1: 0x%16lx\n", c->ttbr0_el1, c->ttbr1_el1);
	pr_notice("vttbr_el2: 0x%16lx   mair_el1: 0x%16lx\n", c->vttbr_el2, c->mair_el1);
	pr_notice("amair_el1: 0x%16lx    tcr_el1: 0x%16lx\n", c->mair_el1, c->tcr_el1);
	pr_notice("  par_el1: 0x%16lx\n", c->par_el1);
}

static int aarch64_vcpu_context_init(struct vmodule *vmodule)
{
	vmodule->context_size = sizeof(struct vcpu_context);
	vmodule->state_init = arch_vcpu_state_init;
	vmodule->state_save = arch_vcpu_state_save;
	vmodule->state_restore = arch_vcpu_state_restore;
	vmodule->state_dump = arch_vcpu_state_dump;

	return 0;
}
MINOS_MODULE_DECLARE(arch_vcpu, "aarch64 vcpu context",
		(void *)aarch64_vcpu_context_init);

static int arm_create_vm(void *item, void *context)
{
	struct vm *vm = item;
	struct arm_virt_data *arch_data;

	arch_data = zalloc(sizeof(struct arm_virt_data));
	if (!arch_data)
		panic("No more memory for arm arch data\n");
	vm->arch_data = arch_data;

	return 0;
}

static int arm_virt_init(void)
{
	return register_hook(arm_create_vm, OS_HOOK_CREATE_VM);
}
module_initcall(arm_virt_init);
