/*
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  1997-01-28  Modified by Finn Arne Gangstad to make timers scale better.
 *
 *  1997-09-10  Updated NTP code according to technical memorandum Jan '96
 *              "A Kernel Model for Precision Timekeeping" by Dave Mills
 *  1998-12-24  Fixed a xtime SMP race (we need the xtime_lock rw spinlock to
 *              serialize accesses to xtime/lost_ticks).
 *                              Copyright (C) 1998  Andrea Arcangeli
 *  1999-03-10  Improved NTP compatibility by Ulrich Windl
 *  2002-05-31	Move sys_sysinfo here and make its locking sane, Robert Love
 *  2000-10-05  Implemented scalable SMP per-CPU timer handling.
 *                              Copyright (C) 2000, 2001, 2002  Ingo Molnar
 */

#include <minos/minos.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/softirq.h>
#include <minos/time.h>
#include <minos/arch.h>

DEFINE_PER_CPU(struct timers, timers);

static void run_timer_softirq(struct softirq_action *h)
{
	struct timer_list *timer;
	unsigned long expires = ~0, now;
	struct timers *timers = &get_cpu_var(timers);
	struct list_head *entry;
	struct list_head *next;
	timer_func_t fn;
	unsigned long data;

	now = NOW();

	/* 
	 * need to aquire the spinlock in case of other
	 * cpu process the list, first thing is to check
	 * whether the timer has been canceled
	 */
	raw_spin_lock(&timers->lock);
	entry = &timers->active;
	next = timers->active.next;

	while ((next != entry) && (next != NULL)) {
		timer = list_entry(next, struct timer_list, entry);
		next = next->next;

		/* 
		 * there may be a issue that when the next entry
		 * has been delete from the timers list, this will cause
		 * that not all the timer can be handled, need to be
		 * fix later, currently only panic here for debug purpose
		 *
		 * here use a member to indicate whether this timer is
		 * requested to be deleted
		 */
		if (atomic_read(&timer->del_request)) {
			pr_debug("timer has been deleted\n");
			list_del(&timer->entry);
			timer->entry.next = NULL;
			timer->expires = (unsigned long)~0;
			continue;
		}

		if (timer->expires <= (now + DEFAULT_TIMER_MARGIN)) {	
			/* 
			 * need to release the spin lock to avoid
			 * dead lock because on the timer handler
			 * function the task may aquire other spinlocks
			 */
			fn = timer->function;
			data = timer->data;

			list_del(&timer->entry);
			timer->entry.next = NULL;
			timers->running_timer = timer;
			raw_spin_unlock(&timers->lock);

			fn(data);

			raw_spin_lock(&timers->lock);
		}

		/* 
		 * need to check whether the timer has been
		 * add to timers list again
		 */
		if ((timer->entry.next != NULL) &&
				(expires > timer->expires))
			expires = timer->expires;
	}

	raw_spin_unlock(&timers->lock);

	if (expires != ((unsigned long)~0)) {
		timers->running_expires = expires;
		enable_timer(expires);
	} else {
		/* there is no more timer on the cpu */
		timers->running_expires = 0;
	}
}

static inline int timer_pending(const struct timer_list * timer)
{
	return ((timer->entry.next) != NULL);
}

static int detach_timer(struct timers *timers, struct timer_list *timer)
{
	struct list_head *entry = &timer->entry;

	if (timer_pending(timer)) {
		list_del(entry);
		entry->next = NULL;
		atomic_set(&timer->del_request, 0);
	}

	return 0;
}

static inline unsigned long slack_expires(unsigned long expires)
{
	return expires;
}

static int __mod_timer(struct timer_list *timer)
{
	unsigned long flags;
	struct timers *timers = timer->timers;

	pr_debug("modify timer to 0x%x\n", timer->expires);

	spin_lock_irqsave(&timers->lock, flags);

	detach_timer(timers, timer);
	list_add_tail(&timers->active, &timer->entry);
	atomic_set(&timer->del_request, 0);

	/*
	 * reprogram the timer for next event do not
	 * need to check the NOW() value, since the timer
	 * also need to trigger event even if the time is
	 * smaller than NOW()
	 */
	if ((timers->running_expires > timer->expires) ||
			(timers->running_expires == 0)) {
		timers->running_expires = timer->expires;
		enable_timer(timers->running_expires);
	}

	spin_unlock_irqrestore(&timers->lock, flags);

	return 0;
}

int mod_timer(struct timer_list *timer, unsigned long expires)
{
	struct timers *timers = timer->timers;
	unsigned long flags;
	int cpu;

	preempt_disable();
	cpu = smp_processor_id();
	expires = slack_expires(expires);
	timer->expires = expires;

	/*
	 * if the timer is not on the current cpu's
	 * timers, need to migrate it to the current
	 * cpu's timers list
	 */
	if (timer->cpu != cpu) {
		spin_lock_irqsave(&timers->lock, flags);
		detach_timer(timers, timer);
		timer->cpu = cpu;
		timer->timers = &get_per_cpu(timers, cpu);
		spin_unlock_irqrestore(&timers->lock, flags);
	}

	__mod_timer(timer);
	preempt_enable();

	return 0;
}

void init_timer_on_cpu(struct timer_list *timer, int cpu)
{
	BUG_ON(!timer);

	preempt_disable();
	init_list(&timer->entry);
	timer->entry.next = NULL;
	timer->expires = 0;
	timer->function = NULL;
	timer->data = 0;
	timer->timers = &get_per_cpu(timers, cpu);
	timer->cpu = cpu;
	preempt_enable();
}

void init_timer(struct timer_list *timer)
{
	preempt_disable();
	BUG_ON(!timer);
	init_timer_on_cpu(timer, smp_processor_id());
	preempt_enable();
}

int del_timer(struct timer_list *timer)
{
	unsigned long flags;
	struct timers *timers = timer->timers;


	if (!timer_pending(timer))
		return 0;

	preempt_disable();

	/* 
	 * if the timer is running on the same cpu
	 * delete it directly otherwise set the del_request
	 * bit
	 */
	if (timer->cpu == smp_processor_id()) {
		spin_lock_irqsave(&timers->lock, flags);
		detach_timer(timers, timer);
		spin_unlock_irqrestore(&timers->lock, flags);
	} else {
		atomic_set(&timer->del_request, 1);
		wmb();
	}

	preempt_enable();

	return 0;
}

void add_timer(struct timer_list *timer)
{
	BUG_ON(timer_pending(timer));
	mod_timer(timer, timer->expires);
}

void init_timers(void)
{
	int i;
	struct timers *timers;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		timers = &get_per_cpu(timers, i);
		init_list(&timers->active);
		timers->running_expires = 0;
		spin_lock_init(&timers->lock);
	}

	open_softirq(TIMER_SOFTIRQ, run_timer_softirq);
}
