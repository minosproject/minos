#ifndef __MINOS_AARCH64_VIRT_H__
#define __MINOS_AARCH64_VIRT_H__

#include <asm/gp_reg.h>

struct vcpu;

struct arm_virt_data {
	int (*dczva_trap)(struct vcpu *vcpu, unsigned long va);
	int (*sgi1r_el1_trap)(struct vcpu *vcpu, unsigned long value);
	int (*phy_timer_trap)(struct vcpu *vcpu, int reg,
			int read, unsigned long *value);
	int (*smc_handler)(struct vcpu *vcpu,
			gp_regs *regs, uint32_t esr);
	int (*hvc_handler)(struct vcpu *vcpu,
			gp_regs *regs, uint32_t esr);
	int (*sysreg_emulation)(struct vcpu *vcpu, int reg,
			int read, unsigned long *value);
};

void set_current_vmid(uint32_t vmid);
uint32_t get_current_vmid(void);
struct vcpu *get_vcpu_from_reg(void);

#endif
