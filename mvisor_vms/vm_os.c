/*
 * created by Le MIn 2017/12/09
 */

#include <mvisor/types.h>
#include <mvisor/vcpu.h>
#include <config/vm_config.h>
#include <mvisor/print.h>

static void os1_boot_vcpu(void *data)
{

}

vm_entry_t vm_os1 = {
	.vmid		= 0,
	.name 		= "os1",
	.nr_vcpu 	= 1,
	.entry_point 	= 0x90000000,
	.vcpu_affinity 	= {0},
	.boot_vcpu 	= os1_boot_vcpu,
	.mmu_on		= 1,
};

vm_entry_t vm_os2 = {
	.vmid		= 1,
	.name 		= "os2",
	.nr_vcpu 	= 4,
	.entry_point 	= 0xa0000000,
	.vcpu_affinity 	= {0},
	.boot_vcpu 	= os1_boot_vcpu,
	.mmu_on		= 1,
};

register_vm_entry(vm_os1);
register_vm_entry(vm_os2);
