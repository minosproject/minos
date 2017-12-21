#include <asm/cpu.h>
#include <core/vcpu.h>

uint64_t sync_el2_handler(vcpu_context_t *context)
{
	int cpuid = get_cpu_id();

	/*
	 * need to store the context to the vcpu in
	 * case there is schedule happen
	 */

	return 0;
}
