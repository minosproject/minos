#include <core/core.h>
#include <core/vm.h>
#include <core/vcpu.h>
#include <core/pcpu.h>
#include <asm/cpu.h>

static int set_up_vcpu_env(struct vmm_vcpu *vcpu)
{
	struct vmm_vcpu_context *c = &vcpu->context;
	uint32_t vmid = get_vm_id(vcpu);
	uint32_t vcpu_id = get_vcpu_id(vcpu);
	uint32_t pcpu_id = get_pcpu_id(vcpu);

	c->x0 = 0;
	c->x1 = 1;
	c->x2 = 2;
	c->x3 = 3;
	c->x4 = 4;
	c->x5 = 5;
	c->x6 = 6;
	c->x7 = 7;
	c->x8 = 8;
	c->x9 = 9;
	c->x10 = 10;
	c->x11 = 11;
	c->x12 = 12;
	c->x13 = 13;
	c->x14 = 14;
	c->x15 = 15;
	c->x16 = 16;
	c->x17 = 17;
	c->x18 = 18;
	c->x19 = 19;
	c->x20 = 20;
	c->x21 = 21;
	c->x22 = 22;
	c->x23 = 23;
	c->x24 = 24;
	c->x25 = 25;
	c->x26 = 26;
	c->x27 = 27;
	c->x28 = 28;
	c->x29 = 29;
	c->x30_lr = 30;
	c->sp_el1 = 0x0;
	c->elr_el2 = vcpu->entry_point;
	c->vbar_el1 = 0;
	c->spsr_el1 = AARCH64_SPSR_EL1h | AARCH64_SPSR_F | \
		      AARCH64_SPSR_I | AARCH64_SPSR_A;
	c->nzcv = 0;
	c->esr_el1 = 0x0;
	c->vmpidr = generate_vcpu_id(vmid, vcpu_id, pcpu_id);

	return 0;
}

struct vmm_vcpu *create_vcpu(struct vmm_vm *vm,
		int index, boot_vm_t func,
		uint32_t affinity, uint64_t entry_point)
{
	struct vmm_vcpu *vcpu;

	vcpu = (struct vmm_vcpu *)request_free_mem(sizeof(struct vmm_vcpu));
	if (vcpu == NULL)
		return NULL;

	memset((char *)vcpu, 0, sizeof(struct vmm_vcpu));

	vcpu->vcpu_id = index;
	vcpu->vm_belong_to = vm;
	vcpu->entry_point = entry_point;
	vcpu->pcpu_affinity = pcpu_affinity(vcpu, affinity);
	if (vcpu->pcpu_affinity == PCPU_AFFINITY_FAIL) {
		pr_fatal("Can not affinity the vcpu %d to pcpu %d\n",
				vcpu->vcpu_id, affinity);
		panic(NULL);
	} else {
		pr_info("Affinity the vcpu %d to pcpu %d\n",
				vcpu->vcpu_id, vcpu->pcpu_affinity);
	}

	set_up_vcpu_env(vcpu);

	if (func)
		func(&vcpu->context);

	return vcpu;
}
