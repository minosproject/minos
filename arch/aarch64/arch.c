#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>
#include <mvisor/vcpu.h>
#include <mvisor/module.h>

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

}

static void aarch64_system_state_save(vcpu_t *vcpu, void *c)
{

}

static void aarch64_system_state_restore(vcpu_t *vcpu, void *c)
{

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
