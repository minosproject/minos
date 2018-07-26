#ifndef _MINOS_TIMER_H_
#define _MINOS_TIMER_H_

/*
 * refer to linux kernel timer code
 */
#include <minos/time.h>

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

#define DEFAULT_TIMER_MARGIN	(128)

void init_timer(struct timer_list *timer);
void init_timer_on_cpu(struct timer_list *timer, int cpu);
void add_timer(struct timer_list *timer);
int del_timer(struct timer_list *timer);
int mod_timer(struct timer_list *timer, unsigned long expires);

#endif
