#ifndef _MINOS_ASM_TIME_H_
#define _MINOS_ASM_TIME_H_

#define HYP_TIMER_INT			(26)
#define VIRT_TIMER_INT			(27)
#define PHYS_TIMER_SEC_INT		(29)
#define PHYS_TIMER_NONSEC_INT		(30)

extern uint64_t boot_tick;
extern uint32_t cpu_khz;

unsigned long get_sys_time();
void arch_enable_timer(unsigned long e);

#endif
