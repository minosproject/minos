#ifndef _MINOS_TIMER_H_
#define _MINOS_TIMER_H_

/*
 * refer to linux kernel timer code
 */
#include <minos/time.h>

typedef void (*timer_func_t)(unsigned long);

struct timer {
	int cpu;
	int stop;
	uint64_t expires;
	uint64_t timeout;
	timer_func_t function;
	unsigned long data;
	struct list_head entry;
	struct raw_timer *raw_timer;
};

/*
 * raw timer is a hardware timer which use to
 * handle timer request.
 */
struct raw_timer {
	struct list_head active;
	unsigned long running_expires;
	struct timer *running_timer;
	spinlock_t lock;
};

void init_timer(struct timer *timer, timer_func_t fn,
		unsigned long data);

int start_timer(struct timer *timer);
int stop_timer(struct timer *timer);
int read_timer(struct timer *timer);
void setup_timer(struct timer *timer, uint64_t tval);
void setup_and_start_timer(struct timer *timer, uint64_t tval);
int mod_timer(struct timer *timer, uint64_t cval);

#endif
