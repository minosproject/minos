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
 *  2018-05-02  Changed for Minos hypervisor
 */

#include <minos/minos.h>
#include <minos/list.h>
#include <minos/timer.h>
#include <minos/softirq.h>
#include <minos/time.h>
#include <minos/arch.h>

#define TIMER_PRECISION 1000000 // 1ms 1000ns

DEFINE_PER_CPU(struct raw_timer, timers);

#define DEFAULT_TIMER_MARGIN	(TIMER_PRECISION / 2)

void soft_timer_interrupt(void)
{
	struct raw_timer *timers = &get_cpu_var(timers);
	struct timer *timer, *next_timer = NULL;
	struct list_head tmp_head;
	uint64_t now;
	timer_func_t fn;
	unsigned long data;

	raw_spin_lock(&timers->lock);
	init_list(&tmp_head);
	now = NOW();

	while (!is_list_empty(&timers->active)) {
		timer = list_first_entry(&timers->active, struct timer, entry);
		if (timer->expires <= (now + DEFAULT_TIMER_MARGIN)) {	
			/* 
			 * need to release the spin lock to avoid
			 * dead lock because on the timer handler
			 * function the task may aquire other spinlocks
			 * so load the function and data on the stack.
			 */
			timers->running_timer = timer;
			smp_wmb();

			fn = timer->function;
			data = timer->data;
			list_del(&timer->entry);
			timer->entry.next = NULL;
			raw_spin_unlock(&timers->lock);

			if (!timer->stop) {
				fn(data);
				mb();
			}

			timers->running_timer = NULL;
			raw_spin_lock(&timers->lock);
		} else {
			/*
			 * need to be careful one case, when do the expires
			 * handler, the spin lock will be released, then other
			 * cpu may get this lock to delete the timer from the
			 * active list, if use the .next member to get the next
			 * timer then there will be issues, so if this timer not
			 * expires, then add it to the tmp timer list head, at
			 * the end of this function, switch the actvie to the head
			 */
			if (!next_timer || (next_timer->expires > timer->expires))
				next_timer = timer;
			list_del(&timer->entry);
			list_add_tail(&tmp_head, &timer->entry);
		}
	}

	if (!is_list_empty(&timers->active))
		panic("error in timers->active list\n");

	/*
	 * swap the tmp head to the active head
	 */
	if (!is_list_empty(&tmp_head)) {
		tmp_head.next->pre = &timers->active;
		timers->active.next = tmp_head.next;
		tmp_head.pre->next = &timers->active;
		timers->active.pre = tmp_head.pre;
		wmb();
	}

	raw_spin_unlock(&timers->lock);

	/*
	 * already in interrupt context, will not be interrupted.
	 */
	timers->next_timer = next_timer;
	if (next_timer)
		enable_timer(next_timer->expires);
}

static inline int timer_pending(const struct timer * timer)
{
	return ((timer->entry.next) != NULL);
}

static int detach_timer(struct raw_timer *timers, struct timer *timer)
{
	struct list_head *entry = &timer->entry;

	if (timer_pending(timer)) {
		list_del(entry);
		entry->next = NULL;
	}

	return 0;
}

static int __mod_timer(struct timer *timer)
{
	struct raw_timer *timers = NULL;
	unsigned long flags;
	int cpu;

	preempt_disable();
	cpu = smp_processor_id();

	/*
	 * if the timer is not on the current cpu's
	 * timers, need to migrate it to the current
	 * cpu's timers list
	 */
	ASSERT(!((timer->cpu != -1) && (timer->cpu != cpu)));
	timers = &get_per_cpu(timers, cpu);

	spin_lock_irqsave(&timers->lock, flags);

	detach_timer(timers, timer);
	timer->stop = 0;
	timer->raw_timer = timers;
	smp_wmb();

	timer->cpu = cpu;
	list_add_tail(&timers->active, &timer->entry);

	/*
	 * reprogram the raw timer if the next expires bigger than
	 * current (expires + DEFAULT_TIMER_MARGIN)
	 */
	if (!timers->next_timer || (timers->next_timer->expires >
				(timer->expires + DEFAULT_TIMER_MARGIN))) {
		timers->next_timer = timer;
		enable_timer(timer->expires);
	}

	spin_unlock_irqrestore(&timers->lock, flags);
	preempt_enable();

	return 0;
}

int mod_timer(struct timer *timer, uint64_t cval)
{
	uint64_t now = NOW();

	if (cval < (now + TIMER_PRECISION))
		timer->expires = now + TIMER_PRECISION;
	else
		timer->expires = cval;

	return __mod_timer(timer);
}

static int __start_delay_timer(struct timer *timer)
{
	if (timer->timeout < TIMER_PRECISION)
		timer->timeout = TIMER_PRECISION;
	timer->expires = NOW() + timer->timeout;

	return __mod_timer(timer);
}

void init_timer(struct timer *timer, timer_func_t fn, unsigned long data)
{
	preempt_disable();
	timer->cpu = -1;
	timer->entry.next = NULL;
	timer->expires = 0;
	timer->timeout = 0;
	timer->function = fn;
	timer->data = data;
	timer->raw_timer = NULL;
	preempt_enable();
}

int start_timer(struct timer *timer)
{
	return __start_delay_timer(timer);
}

void setup_timer(struct timer *timer, uint64_t tval)
{
	timer->timeout = tval;
}

void setup_and_start_timer(struct timer *timer, uint64_t tval)
{
	timer->timeout = tval;
	start_timer(timer);
}

int stop_timer(struct timer *timer)
{
	struct raw_timer *timers = timer->raw_timer;
	unsigned long flags;

	if (timer->cpu == -1)
		return 0;

	timer->stop = 1;
	ASSERT(timer->raw_timer != NULL);
	spin_lock_irqsave(&timers->lock, flags);
	/*
	 * wait the timer finish the action if already
	 * timedout.
	 */
	while (timers->running_timer == timer)
		cpu_relax();

	detach_timer(timers, timer);
	timer->cpu = -1;
	timer->expires = 0;
	spin_unlock_irqrestore(&timers->lock, flags);

	return 0;
}

static int init_raw_timers(void)
{
	struct raw_timer *timers;
	int i;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		timers = &get_per_cpu(timers, i);
		init_list(&timers->active);
		timers->next_timer = NULL;
		timers->running_timer = NULL;
		spin_lock_init(&timers->lock);
	}

	return 0;
}
arch_initcall(init_raw_timers);
