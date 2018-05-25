#ifndef _MINOS_POWER_H_
#define _MINOS_POWER_H_

#define PPOFFR	(0x00)
#define PPONR	(0x04)
#define PCOFFR	(0x08)
#define PWKUPR	(0x0c)
#define PSYSR	(0x10)

void power_on_cpu_core(uint8_t aff0, uint8_t aff1, uint8_t aff2);
void power_off_cpu_core(uint8_t aff0, uint8_t aff1, uint8_t aff2);

#endif
