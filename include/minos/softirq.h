#ifndef _MINOS_SOFTIRQ_H_
#define _MINOS_SOFTIRQ_H_

/*
 * refer to the linux kernel softirq code
 */

enum
{
	TIMER_SOFTIRQ,
	VCPULET_SOFTIRQ,
	SCHED_SOFTIRQ,

	NR_SOFTIRQS
};

struct softirq_action {
	void (*action)(struct softirq_action *);
};

void do_softirq(void);
void open_softirq(int nr, void (*action)(struct softirq_action *));
void softirq_init(void);
void raise_softirq_irqoff(unsigned int nr);
void raise_softirq(unsigned int nr);
void irq_softirq_exit(void);

#endif
