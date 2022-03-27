#ifndef _MINOS_TIME_H_
#define _MINOS_TIME_H_

#include <asm/time.h>
#include <minos/stdlib.h>

#define NOW()			get_sys_time()
#define SYSTEM_TIME_HZ  	1000000000ULL
#define SECONDS(s)     		((uint64_t)((s)  * 1000000000ULL))
#define MILLISECS(ms)  		((uint64_t)((ms) * 1000000ULL))
#define MICROSECS(us)  		((uint64_t)((us) * 1000ULL))

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7
#define CLOCK_REALTIME_ALARM     8
#define CLOCK_BOOTTIME_ALARM     9
#define CLOCK_SGI_CYCLE         10
#define CLOCK_TAI               11

struct timespec {
	long tv_sec;
	long tv_nsec;
};

static inline unsigned long ticks_to_ns(uint64_t ticks)
{
	return muldiv64(ticks, SECONDS(1), 1000 * cpu_khz);
}

static inline uint64_t ns_to_ticks(unsigned long ns)
{
	return muldiv64(ns, 1000 * cpu_khz, SECONDS(1));
}

static inline void enable_timer(unsigned long e)
{
	arch_enable_timer(e);
}

void udelay(uint32_t us);
void mdelay(uint32_t ms);
void msleep(uint32_t ms);

#endif
