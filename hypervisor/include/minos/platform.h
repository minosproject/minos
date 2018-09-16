#ifndef __MINOS_PLATFORM_H__
#define __MINOS_PLATFORM_H__

int platform_serial_init(void);
int platform_cpu_on(int cpu, unsigned long entry);
int platform_time_init(void);

#endif
