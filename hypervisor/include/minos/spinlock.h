/*
 * Created by Le Min 2017/1212
 */

#ifndef _MINOS_SPINLOCK_H_
#define _MINOS_SPINLOCK_H_

#include <asm/arch.h>

typedef struct spinlock {
	volatile uint32_t lock;
} spinlock_t;

#define DEFINE_SPIN_LOCK(name)	\
	spinlock_t name = {	\
		.lock = 0,	\
	}

extern void arch_spin_lock(spinlock_t *lock);
extern void arch_spin_unlock(spinlock_t *lock);

static void inline spin_lock_init(spinlock_t *lock)
{
	lock->lock = 0;
}

static void inline spin_lock(spinlock_t *lock)
{
	arch_spin_lock(lock);
}

static void inline spin_unlock(spinlock_t *lock)
{
	arch_spin_unlock(lock);
}

#define spin_lock_irqsave(l, flags) \
	do { \
		flags = arch_save_irqflags(); \
		arch_disable_local_irq(); \
		arch_spin_lock(l); \
	} while (0)

#define spin_unlock_irqrestore(l, flags) \
	do { \
		arch_spin_unlock(l); \
		arch_restore_irqflags(flags); \
	} while (0)

#endif
