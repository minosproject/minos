#include <core/core.h>
#include <core/vcpu.h>
#include <core/vm.h>
#include <core/pcpu.h>

struct vmm_vcpu *create_vcpu(struct vmm_vm *vm,
		int index, boot_vm_t func, uint32_t affinity)
{
	struct vmm_vcpu *vcpu;

	vcpu = (struct vmm_vcpu *)request_free_mem(sizeof(struct vmm_vcpu));
	if (vcpu == NULL)
		return NULL;

	memset((char *)vcpu, 0, sizeof(struct vmm_vcpu));

	vcpu->vcpu_id = index;
	vcpu->vm_belong_to = vm;
	vcpu->pcpu_affinity = pcpu_affinity(vcpu, affinity);
	if (vcpu->pcpu_affinity == PCPU_AFFINITY_FAIL) {
		pr_fatal("Can not affinity the vcpu %d to pcpu %d\n",
				vcpu->vcpu_id, affinity);
		panic(NULL);
	} else {
		pr_info("Affinity the vcpu %d to pcpu %d\n",
				vcpu->vcpu_id, vcpu->pcpu_affinity);
	}

	func(vm->ram_base, vm->ram_size, &vcpu->context, index);

	return vcpu;
}
