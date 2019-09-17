/*
 * Created by Le Min 2017/1212
 */

#ifndef __MINOS_RAW_SPINLOCK_H__
#define __MINOS_RAW_SPINLOCK_H__

#include <minos/types.h>
#include <minos/atomic.h>

#ifdef CONFIG_SMP
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

static void inline raw_spin_lock(spinlock_t *lock)
{
	int ticket;

	ticket = atomic_add_return_old(1, &lock->next_ticket);
	mb();

	while (ticket != __atomic_get(&lock->ticket_in_service))
		mb();
}

static void inline raw_spin_unlock(spinlock_t *lock)
{
	int ticket;

	ticket = __atomic_get(&lock->ticket_in_service);
	__atomic_set(ticket + 1, &lock->ticket_in_service);
	mb();
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
