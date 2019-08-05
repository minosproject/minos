/*
 * Created by Le Min 2017/1212
 */

#ifndef _MINOS_SPINLOCK_H_
#define _MINOS_SPINLOCK_H_

#include <minos/preempt.h>

#define DEFINE_SPIN_LOCK(name)	\
	spinlock_t name = {	\
		.next_ticket = { \
			.value = 0 \
		}, \
		.ticket_in_service = { \
			.value = 0 \
		} \
	}

static void inline spin_lock_init(spinlock_t *lock)
{
	__atomic_set(0, &lock->next_ticket);
	__atomic_set(0, &lock->ticket_in_service);
}

static void inline __spin_lock(spinlock_t *lock)
{
	int ticket;

	ticket = atomic_add_return_old(1, &lock->next_ticket);
	mb();

	while (ticket != __atomic_get(&lock->ticket_in_service))
		mb();
}

static void inline __spin_unlock(spinlock_t *lock)
{
	int ticket;

	ticket = __atomic_get(&lock->ticket_in_service);
	__atomic_set(ticket + 1, &lock->ticket_in_service);
	mb();
}

#define spin_lock(l) \
	do { \
		preempt_disable(); \
		__spin_lock(l); \
	} while (0)

#define spin_unlock(l)	\
	do { \
		__spin_unlock(l); \
		preempt_enable(); \
	} while (0)

#define spin_lock_irqsave(l, flags) \
	do { \
		flags = arch_save_irqflags(); \
		arch_disable_local_irq(); \
		__spin_lock(l); \
	} while (0)

#define spin_unlock_irqrestore(l, flags) \
	do { \
		__spin_unlock(l); \
		arch_restore_irqflags(flags); \
	} while (0)

#endif
