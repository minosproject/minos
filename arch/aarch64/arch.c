#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>
#include <mvisor/vcpu.h>
#include <mvisor/module.h>
#include <asm/arch.h>

extern int el2_stage2_init(void);
extern int el2_stage1_init(void);

int arch_early_init(void)
{
	el2_stage1_init();
	el2_stage2_init();

	return 0;
}

int arch_init(void)
{
	return 0;
}

struct aarch64_system_context {
	uint64_t vbar_el1;
	uint64_t esr_el1;
	uint64_t vmpidr;
	uint64_t sctlr_el1;
	uint64_t hcr_el2;
} __attribute__ ((__aligned__ (sizeof(unsigned long))));

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
	context->hcr_el2 = 0ul | HCR_EL2_HVC | HCR_EL2_TWI | HCR_EL2_TWE | \
		     HCR_EL2_TIDCP | HCR_EL2_IMO | HCR_EL2_FMO | \
		     HCR_EL2_BSU_IS | HCR_EL2_FB | HCR_EL2_PTW | \
		     HCR_EL2_TSC | HCR_EL2_TACR | HCR_EL2_AMO | \
		     HCR_EL2_RW | HCR_EL2_VM;

	context->vbar_el1 = 0;
	context->esr_el1 = 0;
	context->vmpidr = get_vcpu_id(vcpu);
	context->sctlr_el1 = 0;
}

static void aarch64_system_state_save(struct vcpu *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

	context->vbar_el1 = read_sysreg(VBAR_EL1);
	context->esr_el1 = read_sysreg(ESR_EL1);
	context->vmpidr = read_sysreg(VMPIDR_EL2);
	context->sctlr_el1 = read_sysreg(SCTLR_EL1);
	context->hcr_el2 = read_sysreg(HCR_EL2);
}

static void aarch64_system_state_restore(struct vcpu *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

	write_sysreg(context->vbar_el1, VBAR_EL1);
	write_sysreg(context->esr_el1, ESR_EL1);
	write_sysreg(context->vmpidr, VMPIDR_EL2);
	write_sysreg(context->sctlr_el1, SCTLR_EL1);
	write_sysreg(context->hcr_el2, HCR_EL2);
}

static int aarch64_system_init(struct mvisor_module *module)
{
	module->context_size = sizeof(struct aarch64_system_context);
	module->pdata = NULL;
	module->state_init = aarch64_system_state_init;
	module->state_save = aarch64_system_state_save;
	module->state_restore = aarch64_system_state_restore;

	return 0;
}

MVISOR_MODULE_DECLARE(aarch64_system, "aarch64-system",
	MVISOR_MODULE_NAME_SYSTEM, (void *)aarch64_system_init);
