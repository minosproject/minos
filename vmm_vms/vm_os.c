/*
 * created by Le MIn 2017/12/09
 */

#include <core/vm.h>

static int os1_boot_vm(uint64_t ram_base, uint64_t ram_size,
		struct vmm_vcpu_context *c, uint32_t vcpu_id)
{
	return 0;
}

struct vmm_vm_entry vm_os1 = {
	.name = "os1",
	.ram_base = 0x9000000,
	.ram_size = (256 * 1024 *1024),
	.nr_vcpu = 1,
	.vcpu_affinity = {0},
	.boot_vm = os1_boot_vm,
};

register_vm_entry(vm_os1);
