#ifndef _MVISOR_TIMER_H_
#define _MVISOR_TIMER_H_

/*
 * refer to linux kernel timer code
 */

struct timer_list {
	struct list_head entry;
	unsigned long expires;
	void (*function)(unsigned long);
	unsigned long data;
	struct timers *timers;
};

struct timers {
	struct list_head active;
	unsigned long running_expires;
	struct timer_list *running_timer;
	spinlock_t lock;
};

#define DEFAULT_TIMER_MARGIN	(30)

#endif
