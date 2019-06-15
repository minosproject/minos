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
	atomic_set(&tl->next_ticket, 0);
	atomic_set(&tl->ticket_in_service, 0);
}

static inline void ticket_lock(ticketlock_t *tl)
{
	int ticket;

	preempt_disable();
	ticket = atomic_inc_return_old(&tl->next_ticket);

	while (ticket != atomic_read(&tl->ticket_in_service))
		dsb();
}

static inline void ticket_unlock(ticketlock_t *tl)
{
	int ticket;

	ticket = atomic_read(&tl->ticket_in_service);
	dsb();
	atomic_set(&tl->ticket_in_service, ticket + 1);
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
