#ifndef __MINOS_AARCH64_VIRT_H__
#define __MINOS_AARCH64_VIRT_H__

#include <minos/types.h>

struct vcpu;

struct arm_virt_data {
	int (*dczva_trap)(struct vcpu *vcpu, unsigned long va);
	int (*sgi1r_el1_trap)(struct vcpu *vcpu, unsigned long value);
	int (*phy_timer_trap)(struct vcpu *vcpu, int reg,
			int read, unsigned long *value);
};

#endif
