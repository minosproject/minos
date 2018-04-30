/*
 * created by Le MIn 2017/12/09
 */

#include <mvisor/types.h>
#include <mvisor/vcpu.h>
#include <config/vm_config.h>

static int os1_boot_vm(void *vc)
{
	return 0;
}

vm_entry_t vm_os1 = {
	.vmid		= 0,
	.name 		= "os1",
	.nr_vcpu 	= 4,
	.entry_point 	= 0x90000000,
	.vcpu_affinity 	= {0},
	.boot_vm 	= os1_boot_vm,
	.mmu_on		= 1,
};

vm_entry_t vm_os2 = {
	.vmid		= 1,
	.name 		= "os2",
	.nr_vcpu 	= 4,
	.entry_point 	= 0xa0000000,
	.vcpu_affinity 	= {0},
	.boot_vm 	= os1_boot_vm,
	.mmu_on		= 1,
};

register_vm_entry(vm_os1);
register_vm_entry(vm_os2);
