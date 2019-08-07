/*
 * Created by Le Min 2017/1212
 */

#ifndef __MINOS_SPINLOCK_H__
#define __MINOS_SPINLOCK_H__

#include <minos/preempt.h>
#include <minos/raw_spinlock.h>

#define spin_lock(l) \
	do { \
		preempt_disable(); \
		raw_spin_lock(l); \
	} while (0)

#define spin_unlock(l)	\
	do { \
		raw_spin_unlock(l); \
		preempt_enable(); \
	} while (0)

#define spin_lock_irqsave(l, flags) \
	do { \
		flags = arch_save_irqflags(); \
		arch_disable_local_irq(); \
		raw_spin_lock(l); \
	} while (0)

#define spin_unlock_irqrestore(l, flags) \
	do { \
		raw_spin_unlock(l); \
		arch_restore_irqflags(flags); \
	} while (0)

#endif
