/*
 * Created by Le Min 2017/1212
 */

#ifndef __MINOS_RAW_SPINLOCK_H__
#define __MINOS_RAW_SPINLOCK_H__

#include <minos/types.h>
#include <minos/atomic.h>
#include <asm/asm_current.h>

#ifdef CONFIG_SMP

void arch_ticket_lock(spinlock_t *lock);
void arch_ticket_unlock(spinlock_t *lock);

#define DEFINE_SPIN_LOCK(name)		\
	spinlock_t name = {		\
		.current_ticket = 0,	\
		.next_ticket = 0,	\
	}

static void inline spin_lock_init(spinlock_t *lock)
{
	lock->current_ticket = 0;
	lock->next_ticket = 0;
}

static void inline raw_spin_lock(spinlock_t *lock)
{
	arch_ticket_lock(lock);
}

static void inline raw_spin_unlock(spinlock_t *lock)
{
	arch_ticket_unlock(lock);
}
#else
#define DEFINE_SPIN_LOCK(name) spinlock_t name

static void inline spin_lock_init(spinlock_t *lock)
{

}

static void inline raw_spin_lock(spinlock_t *lock)
{

}

static void inline raw_spin_unlock(spinlock_t *lock)
{

}
#endif

#endif
