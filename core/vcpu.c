#include <core/vcpu.h>
#include <core/print.h>
#include <core/mem_block.h>

struct vmm_vcpu *create_vcpu(struct vmm_vm *vm,
		int index, void *func, uint32_t affinity)
{
	struct vcpu *vcpu;

	vcpu = (struct vmm_vcpu *)request_mem_block(sizeof(struct vmm_vcpu));
	if (vcpu == NULL)
		return NULL;

	vcpu->vcpu_id = index;
	vcpu->vm_belong_to = vm;
	vcpu->hcpu_affinity = hcpu_affinity()
}
