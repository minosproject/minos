#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>

#include <mvisor/vcpu.h>

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
