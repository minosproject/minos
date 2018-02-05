#ifndef _MVISOR_AARCH64_IRQ_H_
#define _MVISOR_AARCH64_IRQ_H_

#include <asm/aarch64_helper.h>

#define NR_LOCAL_IRQS	(32)

#define arch_disable_local_irq()	write_daifset(2)
#define arch_enable_local_irq() 	write_daifclr(2)

static inline unsigned long arch_save_irqflags(void)
{
	return	read_daif();
}

static inline void arch_restore_irqflags(unsigned long flags)
{
	write_daif(flags);
}

#endif
