#ifndef _MINOS_ASM_TIME_H_
#define _MINOS_ASM_TIME_H_

#include <minos/types.h>

extern uint64_t boot_tick;
extern uint32_t cpu_khz;

unsigned long get_sys_time(void);
unsigned long get_current_time(void);
unsigned long get_sys_ticks(void);
void arch_enable_timer(unsigned long e);

#endif
