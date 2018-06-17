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
 *  Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
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
	struct list_head *entry = &timers->active;
	struct list_head *next = timers->active.next;

	now = NOW();

	while (next != entry) {
		timer = list_entry(next, struct timer_list, entry);
		next = next->next;

		if (timer->expires <= (now + DEFAULT_TIMER_MARGIN)) {
			/*
			 * should aquire the spinlock ?
			 * TBD
			 */
			list_del(&timer->entry);
			timer->entry.next = NULL;
			timer->expires = (unsigned long)~0;
			timers->running_timer = timer;
			timer->function(timer->data);

			/*
			 * check whether this timer is add to list
			 * again
			 */
			if (timer->entry.next != NULL)
				expires = timer->expires;
		} else {
			if (expires > timer->expires)
				expires = timer->expires;
		}
	}

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

	if (!timer_pending(timer))
		return 0;

	list_del(entry);
	entry->next = NULL;

	return 0;
}

static inline unsigned long slack_expires(unsigned long expires)
{
	return expires;
}

int mod_timer(struct timer_list *timer, unsigned long expires)
{
	unsigned long flags;
	struct timers *timers = timer->timers;

	/*
	 * if the timer is on pending on the timers active list
	 * and the new expires equal the timer->expires, just
	 * return
	 */
	if ((timer_pending(timer)) && (timer->expires == expires))
		return 1;

	pr_debug("modify timer to 0x%x\n", expires);

	spin_lock_irqsave(&timers->lock, flags);

	expires = slack_expires(expires);
	timer->expires = expires;

	detach_timer(timers, timer);
	list_add_tail(&timers->active, &timer->entry);

	/*
	 * reprogram the timer for next event
	 */
	if ((timers->running_expires > expires) ||
			(timers->running_expires == 0)) {
		timers->running_expires = expires;
		enable_timer(timers->running_expires);
	}

	spin_unlock_irqrestore(&timers->lock, flags);

	return 0;
}

void init_timer_on_cpu(struct timer_list *timer, int cpu)
{
	BUG_ON(!timer);

	init_list(&timer->entry);
	timer->entry.next = NULL;
	timer->expires = 0;
	timer->function = NULL;
	timer->data = 0;
	timer->timers = &get_per_cpu(timers, cpu);
}

void init_timer(struct timer_list *timer)
{
	BUG_ON(!timer);

	init_list(&timer->entry);
	timer->entry.next = NULL;
	timer->expires = 0;
	timer->function = NULL;
	timer->data = 0;
	timer->timers = &get_cpu_var(timers);
}

int del_timer(struct timer_list *timer)
{
	unsigned long flags;
	struct timers *timers = timer->timers;

	if (timer->entry.next == NULL)
		return 0;

	spin_lock_irqsave(&timers->lock, flags);
	detach_timer(timers, timer);
	spin_unlock_irqrestore(&timers->lock, flags);

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
