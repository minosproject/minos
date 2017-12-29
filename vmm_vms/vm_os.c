/*
 * created by Le MIn 2017/12/09
 */

#include <core/types.h>
#include <core/vcpu.h>
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
};

register_vm_entry(vm_os1);
