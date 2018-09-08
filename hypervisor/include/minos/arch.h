#ifndef __MINOS_ARCH_H_
#define __MINOS_ARCH_H_

#include <asm/arch.h>

struct vcpu;

int smp_processor_id(void);
int arch_early_init(void *data);
int __arch_init(void);
void arch_init_vcpu(struct vcpu *vcpu, void *entry);

int cpu_on(int cpu, unsigned long entry);

#endif
