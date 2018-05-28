#ifndef __MINOS_VIRT_H_
#define __MINOS_VIRT_H_

#include <virt/vcpu.h>
#include <minos/arch.h>

void exit_from_guest(gp_regs *regs);
void enter_to_guest(gp_regs *regs);

void vcpu_idle(struct vcpu *vcpu);
void vcpu_online(struct vcpu *vcpu);
void vcpu_offline(struct vcpu *vcpu);
int vcpu_power_on(struct vcpu *caller, int cpuid,
		unsigned long entry, unsigned long unsed);

#endif
