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

#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>
#include <minos/vcpu.h>
#include <minos/vmodule.h>
#include <asm/arch.h>
#include <minos/string.h>
#include <minos/print.h>
#include <minos/sched.h>
#include <minos/calltrace.h>
#include <minos/smp.h>

extern int el2_stage2_init(void);
extern int el2_stage1_init(void);
extern int of_init(void *setup_data);
extern void hvm_dtb_init(struct vm *vm);

static void dump_register(gp_regs *regs)
{
	unsigned long spsr;

	if (!regs)
		return;

	spsr = regs->spsr_elx;
	pr_fatal("SPSR:0x%x Mode:%d-%s F:%d I:%d A:%d D:%d NZCV:%x\n",
			spsr, (spsr & 0x7), (spsr & 0x8) ?
			"aarch64" : "aarch32", (spsr & (BIT(6))) >> 6,
			(spsr & (BIT(7))) >> 7, (spsr & (BIT(8))) >> 8,
			(spsr & (BIT(9))) >> 9, spsr >> 28);

	pr_fatal("x0:0x%p x1:0x%p x2:0x%p\n",
			regs->x0, regs->x1, regs->x2);
	pr_fatal("x3:0x%p x4:0x%p x5:0x%p\n",
			regs->x3, regs->x4, regs->x5);
	pr_fatal("x6:0x%p x7:0x%p x8:0x%p\n",
			regs->x6, regs->x7, regs->x8);
	pr_fatal("x9:0x%p x10:0x%p x11:0x%p\n",
			regs->x9, regs->x10, regs->x11);
	pr_fatal("x12:0x%p x13:0x%p x14:0x%p\n",
			regs->x12, regs->x13, regs->x14);
	pr_fatal("x15:0x%p x16:0x%p x17:0x%p\n",
			regs->x15, regs->x16, regs->x17);
	pr_fatal("x18:0x%p x19:0x%p x20:0x%p\n",
			regs->x18, regs->x19, regs->x20);
	pr_fatal("x21:0x%p x22:0x%p x23:0x%p\n",
			regs->x21, regs->x22, regs->x23);
	pr_fatal("x24:0x%p x25:0x%p x26:0x%p\n",
			regs->x24, regs->x25, regs->x26);
	pr_fatal("x27:0x%p x28:0x%p x29:0x%p\n",
			regs->x27, regs->x28, regs->x29);
	pr_fatal("lr:0x%p esr:0x%p spsr:0x%p\n",
			regs->lr, regs->esr_elx, regs->spsr_elx);
	pr_fatal("elr:0x%p\n", regs->elr_elx);
}

void arch_dump_stack(gp_regs *regs, unsigned long *stack)
{
	struct vcpu *vcpu = current_vcpu;
	extern unsigned char __el2_stack_end;
	unsigned long stack_base;
	unsigned long fp, lr = 0;

	if ((vcpu) && !(vcpu->is_idle)) {
		pr_fatal("current vcpu: vmid:%d vcpuid:%d vcpu_name:%s\n",
				get_vmid(vcpu), get_vcpu_id(vcpu), vcpu->name);
		stack_base = (unsigned long)vcpu->stack_origin;
	} else {
		stack_base = (unsigned long)&__el2_stack_end -
			(smp_processor_id() << IDLE_VCPU_STATCK_SHIFT);
	}

	dump_register(regs);

	if (!stack) {
		if (regs) {
			fp = regs->x29;
			lr = regs->elr_elx;
		} else {
			fp = arch_get_fp();
			lr = arch_get_lr();
		}
	} else {
		fp = *stack;
	}

	pr_fatal("Call Trace :\n");
	pr_fatal("------------ cut here ------------\n");
	do {
		print_symbol(lr);

		if ((fp < (stack_base - VCPU_DEFAULT_STACK_SIZE))
				|| (fp >= stack_base))
				break;

		lr = *(unsigned long *)(fp + sizeof(unsigned long));
		lr -= 4;
		fp = *(unsigned long *)fp;
	} while (1);
}

int arch_taken_from_guest(gp_regs *regs)
{
	/* TBD */
	return ((regs->spsr_elx & 0xf) != (AARCH64_SPSR_EL2h));
}

static inline void
arch_init_gp_regs(gp_regs *regs, void *entry, int idle, int bit64)
{
	regs->elr_elx = (uint64_t)entry;

	if (idle)
		regs->spsr_elx = AARCH64_SPSR_EL2h | AARCH64_SPSR_F | \
				 AARCH64_SPSR_I | AARCH64_SPSR_A;
	else {
		if (bit64)
			regs->spsr_elx = AARCH64_SPSR_EL1h | \
				AARCH64_SPSR_F | \
				AARCH64_SPSR_I | \
				AARCH64_SPSR_A;
		else
			regs->spsr_elx = AARCH32_SVC | \
				AARCH64_SPSR_F | \
				AARCH64_SPSR_I | \
				AARCH64_SPSR_A | (1 << 4);
	}
}

void arch_init_vcpu(struct vcpu *vcpu, void *entry)
{
	gp_regs *regs;
	struct vm *vm = vcpu->vm;

	regs = stack_to_gp_regs(vcpu->stack_origin);
	memset((char *)regs, 0, sizeof(gp_regs));
	vcpu->stack_base = vcpu->stack_origin - sizeof(gp_regs);

	arch_init_gp_regs(regs, entry, vcpu->is_idle, vm_is_64bit(vm));
}

int arch_early_init(void *setup_data)
{
	pr_info("current EL is 0x%x\n", GET_EL(read_CurrentEl()));

	if (!IS_IN_EL2())
		panic("minos must run at EL2 mode\n");

	el2_stage2_init();
	of_init(setup_data);

	return 0;
}

int __arch_init(void)
{
	return 0;
}

void arch_hvm_init(struct vm *vm)
{
	hvm_dtb_init(vm);
}

static int aarch64_init_percpu(void)
{
	/*
	 * set IMO and FMO let physic irq and fiq taken to
	 * EL2, without this irq and fiq will not send to
	 * the cpu
	 */
	write_sysreg64(HCR_EL2_IMO | HCR_EL2_FMO, HCR_EL2);
	dsb();

	return 0;
}
arch_initcall_percpu(aarch64_init_percpu);

struct aarch64_system_context {
	uint64_t vbar_el1;
	uint64_t esr_el1;
	uint64_t sp_el1;
	uint64_t sp_el0;
	uint64_t elr_el1;
	uint64_t vmpidr;
	uint64_t vpidr;
	uint64_t sctlr_el1;
	uint64_t hcr_el2;
	uint64_t spsr_el1;
	uint64_t far_el1;
	uint64_t actlr_el1;
	uint64_t tpidr_el1;
	uint64_t csselr;
	uint64_t cpacr;
	uint64_t contextidr;
	uint64_t tpidr_el0;
	uint64_t tpidrro_el0;
	uint64_t cntkctl;
	uint64_t afsr0;
	uint64_t afsr1;
	uint32_t teecr;
	uint32_t teehbr;
	uint32_t dacr32_el2;
	uint32_t ifsr32_el2;
}__align(sizeof(unsigned long));

static void aarch64_system_state_init(struct vcpu *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

	memset(context, 0, sizeof(struct aarch64_system_context));

	/*
	 * HVC : enable hyper call function
	 * TWI : trap wfi
	 * TWE : trap wfe
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
	context->hcr_el2 = 0ul | HCR_EL2_HVC | HCR_EL2_TWI | \
		     HCR_EL2_TIDCP | HCR_EL2_IMO | HCR_EL2_FMO | \
		     HCR_EL2_BSU_IS | HCR_EL2_FB | HCR_EL2_PTW | \
		     HCR_EL2_TSC | HCR_EL2_TACR | HCR_EL2_AMO | \
		     HCR_EL2_VM;

	if (vm_is_64bit(vcpu->vm))
		context->hcr_el2 |= HCR_EL2_RW;

	context->vmpidr = 0x00000000 | get_vcpu_id(vcpu);
	context->vpidr = 0x410fc050;	/* arm fvp */
}

static void aarch64_system_state_save(struct vcpu *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

	dsb();
	context->vbar_el1 = read_sysreg(VBAR_EL1);
	context->esr_el1 = read_sysreg(ESR_EL1);
	context->elr_el1 = read_sysreg(ELR_EL1);
	context->vmpidr = read_sysreg(VMPIDR_EL2);
	context->vpidr = read_sysreg(VPIDR_EL2);
	context->sctlr_el1 = read_sysreg(SCTLR_EL1);
	context->hcr_el2 = read_sysreg(HCR_EL2);
	context->sp_el1 = read_sysreg(SP_EL1);
	context->sp_el0 = read_sysreg(SP_EL0);
	context->spsr_el1 = read_sysreg(SPSR_EL1);
	context->far_el1 = read_sysreg(FAR_EL1);
	context->actlr_el1 = read_sysreg(ACTLR_EL1);
	context->tpidr_el1 = read_sysreg(TPIDR_EL1);
	context->csselr = read_sysreg(CSSELR_EL1);
	context->cpacr = read_sysreg(CPACR_EL1);
	context->contextidr = read_sysreg(CONTEXTIDR_EL1);
	context->tpidr_el0 = read_sysreg(TPIDR_EL0);
	context->tpidrro_el0 = read_sysreg(TPIDRRO_EL0);
	context->cntkctl = read_sysreg(CNTKCTL_EL1);
	context->afsr0 = read_sysreg(AFSR0_EL1);
	context->afsr1 = read_sysreg(AFSR1_EL1);

	if (vm_is_32bit(vcpu->vm)) {
		//context->teecr = read_sysreg32(TEECR32_EL1);
		//context->teehbr = read_sysreg32(TEEHBR32_EL1);
		context->dacr32_el2 = read_sysreg32(DACR32_EL2);
		context->ifsr32_el2 = read_sysreg32(IFSR32_EL2);
	}
}

static void aarch64_system_state_restore(struct vcpu *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

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

	if (vm_is_32bit(vcpu->vm)) {
		//write_sysreg(context->teecr, TEECR32_EL1);
		//write_sysreg(context->teehbr, TEEHBR32_EL1);
		write_sysreg(context->dacr32_el2, DACR32_EL2);
		write_sysreg(context->ifsr32_el2, IFSR32_EL2);
	}

	dsb();
}

static int aarch64_system_init(struct vmodule *vmodule)
{
	vmodule->context_size = sizeof(struct aarch64_system_context);
	vmodule->pdata = NULL;
	vmodule->state_init = aarch64_system_state_init;
	vmodule->state_save = aarch64_system_state_save;
	vmodule->state_restore = aarch64_system_state_restore;

	return 0;
}

MINOS_MODULE_DECLARE(aarch64_system,
	"aarch64-system", (void *)aarch64_system_init);
