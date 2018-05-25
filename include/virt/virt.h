#ifndef __MINOS_VIRT_H_
#define __MINOS_VIRT_H_

void exit_from_guest(struct vcpu *vcpu);
void enter_to_guest(struct vcpu *vcpu);

void vcpu_idle(struct vcpu *vcpu);
void vcpu_online(struct vcpu *vcpu);
void vcpu_offline(struct vcpu *vcpu);
int vcpu_power_on(struct vcpu *caller, int cpuid,
		unsigned long entry, unsigned long unsed);

#endif
