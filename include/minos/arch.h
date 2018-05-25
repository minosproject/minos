#ifndef __MINOS_ARCH_H_
#define __MINOS_ARCH_H_

#include <asm/arch.h>

struct task;

int get_cpu_id(void);
int arch_early_init(void);
int __arch_init(void);
void arch_init_task(struct task *task, void *entry);

#endif
