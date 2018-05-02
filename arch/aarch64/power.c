#include <mvisor/mvisor.h>
#include <asm/power.h>
#include <mvisor/io.h>

static void *pc_base = (void *)0x1c10000;

void power_on_cpu_core(uint8_t aff0,
		uint8_t aff1, uint8_t aff2)
{
	uint32_t mpid = 0;

	mpid = (aff0 << 0) | (aff1 << 8) | (aff2 << 16);
	iowrite32(pc_base + PPONR, mpid);
}

void power_off_cpu_core(uint8_t aff0,
		uint8_t aff1, uint8_t aff2)
{
	uint32_t mpid = 0;

	mpid = (aff0 << 0) | (aff1 << 8) | (aff2 << 16);
	iowrite32(pc_base + PPOFFR, mpid);
}
