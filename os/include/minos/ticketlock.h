#ifndef __MINOS_TICKETLOCK_H__
#define __MINOS_TICKETLOCK_H__

#include <minos/ticketlock.h>
#include <minos/atomic.h>
#include <asm/arch.h>

typedef struct ticketlock {
	atomic_t next_ticket;
	atomic_t ticket_in_service;
} ticketlock_t;

#define DEFINE_TICKET_LOCK(name) \
	ticketlock_t name = {	 \
		.next_ticket = { \
			.value = 0 \
		}, \
		.ticket_in_service = { \
			.value = 0 \
		} \
	}

static inline void ticketlock_init(ticketlock_t *tl)
{
	__atomic_set(0, &tl->next_ticket);
	__atomic_set(0, &tl->ticket_in_service);
}

static inline void ticket_lock(ticketlock_t *tl)
{
	int ticket;

	preempt_disable();
	ticket = atomic_add_return_old(1, &tl->next_ticket);
	mb();

	while (ticket != __atomic_get(&tl->ticket_in_service))
		mb();
}

static inline void ticket_unlock(ticketlock_t *tl)
{
	int ticket;

	ticket = __atomic_get(&tl->ticket_in_service);
	mb();
	__atomic_set(ticket + 1, &tl->ticket_in_service);
	preempt_enable();
}

#define ticket_lock_irqsave(l, flags) \
	do { \
		flags = arch_save_irqflags(); \
		arch_disable_local_irq(); \
		ticket_lock(l); \
	} while (0)

#define ticket_unlock_irqrestore(l, flags) \
	do { \
		ticket_unlock(l); \
		arch_restore_irqflags(flags); \
	} while (0)

#endif
