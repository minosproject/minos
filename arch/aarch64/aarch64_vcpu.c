#include <asm/aarch64_common.h>
#include <asm/aarch64_helper.h>

#include <mvisor/vcpu.h>

int arch_vm_init(vm_t *vm)
{
	int i;
	vcpu_t *vcpu;
	vcpu_regs *regs;

	for (i = 0; i < vm->vcpu_nr; i++) {
		vcpu = vm->vcpus[i];
		regs = &vcpu->regs;

		regs->x0 = 0x0;
		regs->x1 = 0x1;
		regs->x2 = 0x2;
		regs->x3 = 0x3;
		regs->x4 = 0x4;
		regs->x5 = 0x5;
		regs->x6 = 0x6;
		regs->x7 = 0x7;
		regs->x8 = 0x8;
		regs->x9 = 0x9;
		regs->x10 = 0x10;
		regs->x11 = 0x11;
		regs->x12 = 0x12;
		regs->x13 = 0x13;
		regs->x14 = 0x14;
		regs->x15 = 0x15;
		regs->x16 = 0x16;
		regs->x17 = 0x17;
		regs->x18 = 0x18;
		regs->x19 = 0x19;
		regs->x20 = 0x20;
		regs->x21 = 0x21;
		regs->x22 = 0x22;
		regs->x23 = 0x23;
		regs->x24 = 0x24;
		regs->x25 = 0x25;
		regs->x26 = 0x26;
		regs->x27 = 0x27;
		regs->x28 = 0x28;
		regs->x29 = 0x29;
		regs->x30_lr = 0x30;
		regs->sp_el1 = 0x0;
		regs->elr_el2 = vcpu->entry_point;
		regs->spsr_el2 = AARCH64_SPSR_EL1h | AARCH64_SPSR_F | \
				 AARCH64_SPSR_I | AARCH64_SPSR_A;
		regs->nzcv = 0;
	}

	return 0;
}
