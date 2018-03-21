#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>
#include <mvisor/vcpu.h>
#include <mvisor/module.h>
#include <asm/arch.h>

extern int el2_stage2_vmsa_init(void);

int arch_early_init(void)
{
	return 0;
}

int arch_init(void)
{
	el2_stage2_vmsa_init();

	return 0;
}

struct aarch64_system_context {
	uint64_t vbar_el1;
	uint64_t esr_el1;
	uint64_t vmpidr;
	uint64_t sctlr_el1;
	uint64_t hcr_el2;
} __attribute__ ((__aligned__ (sizeof(unsigned long))));

static void aarch64_system_state_init(vcpu_t *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

	context->hcr_el2 = 0ul | HCR_EL2_HVC | HCR_EL2_TWI | HCR_EL2_TWE | \
		     HCR_EL2_TIDCP | HCR_EL2_IMO | HCR_EL2_FMO | \
		     HCR_EL2_AMO | HCR_EL2_RW | HCR_EL2_VM;
	context->vbar_el1 = 0;
	context->esr_el1 = 0;
	context->vmpidr = get_vcpu_id(vcpu);
	context->sctlr_el1 = 0;
}

static void aarch64_system_state_save(vcpu_t *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

	context->vbar_el1 = read_sysreg(VBAR_EL1);
	context->esr_el1 = read_sysreg(ESR_EL1);
	context->vmpidr = read_sysreg(VMPIDR_EL2);
	context->sctlr_el1 = read_sysreg(SCTLR_EL1);
	context->hcr_el2 = read_sysreg(HCR_EL2);
}

static void aarch64_system_state_restore(vcpu_t *vcpu, void *c)
{
	struct aarch64_system_context *context =
			(struct aarch64_system_context *)c;

	write_sysreg(context->vbar_el1, VBAR_EL1);
	write_sysreg(context->esr_el1, ESR_EL1);
	write_sysreg(context->vmpidr, VMPIDR_EL2);
	write_sysreg(context->sctlr_el1, SCTLR_EL1);
	write_sysreg(context->hcr_el2, HCR_EL2);
}

static int aarch64_system_init(struct vmm_module *module)
{
	module->context_size = sizeof(struct aarch64_system_context);
	module->pdata = NULL;
	module->state_init = aarch64_system_state_init;
	module->state_save = aarch64_system_state_save;
	module->state_restore = aarch64_system_state_restore;

	return 0;
}

VMM_MODULE_DECLARE(aarch64_system, "aarch64-system",
	VMM_MODULE_NAME_SYSTEM, (void *)aarch64_system_init);
