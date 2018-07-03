#ifndef __MINOS_ARCH_H_
#define __MINOS_ARCH_H_

#include <asm/arch.h>

struct vcpu;

int get_cpu_id(void);
int arch_early_init(void);
int __arch_init(void);
void arch_init_vcpu(struct vcpu *vcpu, void *entry);

#endif
