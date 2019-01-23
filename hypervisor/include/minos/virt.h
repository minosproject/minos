#ifndef __MINOS_VIRT_H_
#define __MINOS_VIRT_H_

#include <minos/minos.h>
#include <minos/arch.h>
#include <minos/vcpu.h>
#include <minos/vmodule.h>

static inline int taken_from_guest(gp_regs *regs)
{
	return arch_taken_from_guest(regs);
}

static inline void exit_from_guest(struct vcpu *vcpu, gp_regs *regs)
{
	do_hooks((void *)vcpu, (void *)regs,
			MINOS_HOOK_TYPE_EXIT_FROM_GUEST);
}

static inline void enter_to_guest(struct vcpu *vcpu, gp_regs *regs)
{
	do_hooks((void *)vcpu, (void *)regs,
			MINOS_HOOK_TYPE_ENTER_TO_GUEST);
}

static inline void save_vcpu_state(struct vcpu *vcpu)
{
	save_vcpu_vmodule_state(vcpu);
}

static inline void restore_vcpu_state(struct vcpu *vcpu)
{
	restore_vcpu_vmodule_state(vcpu);
}

#endif
