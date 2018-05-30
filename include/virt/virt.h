#ifndef __MINOS_VIRT_H_
#define __MINOS_VIRT_H_

#include <virt/vcpu.h>
#include <minos/arch.h>

int taken_from_guest(gp_regs *regs);

void exit_from_guest(struct task *task, gp_regs *regs);
void enter_to_guest(struct task *task, gp_regs *regs);

void save_vcpu_task_state(struct task *task);
void restore_vcpu_task_state(struct task *task);

void vcpu_idle(struct vcpu *vcpu);
void vcpu_online(struct vcpu *vcpu);
void vcpu_offline(struct vcpu *vcpu);
int vcpu_power_on(struct vcpu *caller, int cpuid,
		unsigned long entry, unsigned long unsed);

#endif
