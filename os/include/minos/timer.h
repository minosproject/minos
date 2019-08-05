#ifndef _MINOS_TIMER_H_
#define _MINOS_TIMER_H_

/*
 * refer to linux kernel timer code
 */
#include <minos/time.h>
#include <minos/os_def.h>

#define DEFAULT_TIMER_MARGIN	(10)

void init_timer(struct timer_list *timer);
void init_timer_on_cpu(struct timer_list *timer, int cpu);
void add_timer(struct timer_list *timer);
int del_timer(struct timer_list *timer);
int mod_timer(struct timer_list *timer, unsigned long expires);

#endif
