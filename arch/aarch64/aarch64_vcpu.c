#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>

#include <mvisor/vcpu.h>

int arch_vm_init(struct vm *vm)
{
	int i;
	struct vcpu *vcpu;
	vcpu_regs *regs;

	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = vm->vcpus[i];
		regs = &vcpu->regs;

		memset((char *)regs, 0, sizeof(vcpu_regs));

		regs->elr_el2 = vcpu->entry_point;
		regs->spsr_el2 = AARCH64_SPSR_EL1h | AARCH64_SPSR_F | \
				 AARCH64_SPSR_I | AARCH64_SPSR_A;
		regs->nzcv = 0;
	}

	return 0;
}
