#ifndef _MVISOR_ASM_VCPU_H_
#define _MVISOR_ASM_VCPU_H_

#include <mvisor/types.h>

typedef struct aarch64_vcpu_regs {
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
	uint64_t x30_lr;
	uint64_t sp_el1;
	uint64_t elr_el2;
	uint64_t spsr_el2;
	uint64_t nzcv;
	uint64_t esr_el2;
} vcpu_regs __attribute__ ((__aligned__ (sizeof(unsigned long))));

static inline unsigned long
get_reg_value(vcpu_regs *regs, uint32_t index)
{
	unsigned long *base = (unsigned long *)regs;

	if (index > 30)
		return 0;

	return *(base + index);
}

static inline void
set_reg_value(vcpu_regs *regs, uint32_t index, unsigned long value)
{
	unsigned long *base = (unsigned long *)regs;

	if (index > 30)
		return;

	*(base + index) = value;
}

#endif
