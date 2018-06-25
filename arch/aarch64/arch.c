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
#include <virt/vcpu.h>
#include <virt/vmodule.h>
#include <asm/arch.h>
#include <minos/task.h>
#include <minos/string.h>

extern int el2_stage2_init(void);
extern int el2_stage1_init(void);

int arch_taken_from_guest(gp_regs *regs)
{
	/* TBD */
	return ((regs->spsr_elx & 0xf) != (AARCH64_SPSR_EL2h));
}

static inline void arch_init_gp_regs(gp_regs *regs, void *entry, int type)
{
	regs->elr_elx = (uint64_t)entry;

	if (type != TASK_TYPE_VCPU)
		regs->spsr_elx = AARCH64_SPSR_EL2h | AARCH64_SPSR_F | \
				 AARCH64_SPSR_I | AARCH64_SPSR_A;
	else

		regs->spsr_elx = AARCH64_SPSR_EL1h | AARCH64_SPSR_F | \
				 AARCH64_SPSR_I | AARCH64_SPSR_A;
}

void arch_init_task(struct task *task, void *entry)
{
	gp_regs *regs;

	regs = stack_to_gp_regs(task->stack_origin);
	memset((char *)regs, 0, sizeof(gp_regs));
	task->stack_base = task->stack_origin - sizeof(gp_regs);

	arch_init_gp_regs(regs, entry, task->task_type);
}

int arch_early_init(void)
{
	el2_stage1_init();
	el2_stage2_init();

	return 0;
}

int __arch_init(void)
{
	return 0;
}

struct aarch64_system_context {
	uint64_t vbar_el1;
	uint64_t esr_el1;
	uint64_t sp_el1;
	uint64_t sp_el0;
	uint64_t elr_el1;
	uint64_t vmpidr;
	uint64_t sctlr_el1;
	uint64_t hcr_el2;
	uint64_t spsr_el1;
	uint64_t far_el1;
	uint64_t actlr_el1;
	uint64_t tpidr_el1;
}__align(sizeof(unsigned long));

static void aarch64_system_state_init(struct vcpu *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

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
		     HCR_EL2_RW | HCR_EL2_VM;

	context->vbar_el1 = 0;
	context->esr_el1 = 0;
	context->elr_el1 = 0;
	context->vmpidr = get_vcpu_id(vcpu);
	context->sctlr_el1 = 0;
	context->sp_el1 = 0;
	context->sp_el0 = 0;
	context->spsr_el1 = 0;
	context->far_el1 = 0;
	context->actlr_el1 = 0;
	context->tpidr_el1 = 0;
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
	context->sctlr_el1 = read_sysreg(SCTLR_EL1);
	context->hcr_el2 = read_sysreg(HCR_EL2);
	context->sp_el1 = read_sysreg(SP_EL1);
	context->sp_el0 = read_sysreg(SP_EL0);
	context->spsr_el1 = read_sysreg(SPSR_EL1);
	context->far_el1 = read_sysreg(FAR_EL1);
	context->actlr_el1 = read_sysreg(ACTLR_EL1);
	context->tpidr_el1 = read_sysreg(TPIDR_EL1);
}

static void aarch64_system_state_restore(struct vcpu *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

	write_sysreg(context->vbar_el1, VBAR_EL1);
	write_sysreg(context->esr_el1, ESR_EL1);
	write_sysreg(context->elr_el1, ELR_EL1);
	write_sysreg(context->vmpidr, VMPIDR_EL2);
	write_sysreg(context->sctlr_el1, SCTLR_EL1);
	write_sysreg(context->hcr_el2, HCR_EL2);
	write_sysreg(context->sp_el1, SP_EL1);
	write_sysreg(context->sp_el0, SP_EL0);
	write_sysreg(context->spsr_el1, SPSR_EL1);
	write_sysreg(context->far_el1, FAR_EL1);
	write_sysreg(context->actlr_el1, ACTLR_EL1);
	write_sysreg(context->tpidr_el1, TPIDR_EL1);
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
