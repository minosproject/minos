#include <core/core.h>
#include <core/vcpu.h>
#include <core/vm.h>

extern uint64_t vmm_vm_start;
extern uint64_t vmm_vm_end;

static struct vmm_vm *vms = NULL;

void init_vms(void)
{
	int i, j;
	uint64_t size = vmm_vm_end - vmm_vm_start;
	unsigned long *start = (unsigned long *)vmm_vm_start;
	struct vmm_vm *vm;
	struct vmm_vm_entry *vme;
	struct vmm_vcpu *vcpu;

	if (size == 0)
		panic("No VM is found\n");

	size = size / sizeof(struct vmm_vm_entry *);
	pr_debug("Found %d VMs config\n", size);

	vms = (struct vmm_vm *)request_free_mem(size * sizeof(struct vmm_vm));
	if (NULL == vms)
		panic("No more memory for vms\n");

	memset((char *)vms, 0, size * sizeof(struct vmm_vm));

	for (i = 0; i < size; i++) {
		vme = (struct vmm_vm_entry *)(*start);
		vm = &vms[i];
		pr_info("found vm-%d %s ram_base:0x%x ram_size:0x%x nr_vcpu:%d\n",
			i, vme->name, vme->ram_base, vme->ram_size, vme->nr_vcpu);

		vm->vmid = i;
		strncpy(vm->name, vme->name,
			MIN(strlen(vme->name), VMM_VM_NAME_SIZE - 1));
		vm->ram_base = vme->ram_base;
		vm->ram_size = vme->ram_size;
		vm->vcpu_nr = MIN(vme->nr_vcpu, VM_MAX_VCPU);

		for (j = 0; j < vm->vcpu_nr; j++)
		{
			vcpu = create_vcpu(vm, j, vme->boot_vm,
					vme->vcpu_affinity[j]);
			if (NULL == vcpu)
				panic("No more memory to create VCPU\n");
			vm->vcpus[j] = vcpu;
		}

		start++;
	}
}
