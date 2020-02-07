#ifndef __MINOS_AARCH64_VIRT_H__
#define __MINOS_AARCH64_VIRT_H__

#include <minos/types.h>

/*
 * in hypervisor:
 * elr_elx equals to elr_el2
 * spsr_elx equals to spsr_el2
 * esr_elx equals to esr_el2
 *
 * these register of EL1 will save and restore by
 * arch/aarch64/virt/arch_virt.c
 */
typedef struct aarch64_regs {
	uint64_t elr_elx;
	uint64_t spsr_elx;
	uint64_t esr_elx;
	uint64_t x0;
	uint64_t x1;
	uint64_t x2;
	uint64_t x3;
	uint64_t x4;
	uint64_t x5;
	uint64_t x6;
	uint64_t x7;
	uint64_t x8;
	uint64_t x9;
	uint64_t x10;
	uint64_t x11;
	uint64_t x12;
	uint64_t x13;
	uint64_t x14;
	uint64_t x15;
	uint64_t x16;
	uint64_t x17;
	uint64_t x18;
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t lr;
} gp_regs __align(sizeof(uint64_t));

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

#endif
