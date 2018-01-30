#include <core/types.h>
#include <core/io.h>
#include "gicv3_gicc.h"
#include <asm/cpu.h>

void send_sgi_int_to_cpu(uint32_t cpu, uint8_t intid)
{
	uint8_t aff0, aff1, aff2, aff3;

	aff0 = (uint8_t)cpu;
	aff1 = (uint8_t)cpu >> 8;
	aff2 = (uint8_t)cpu >> 16;
	aff3 = (uint8_t)cpu >> 24;

	set_icc_sgi1r(aff3, aff2, aff1, 0, aff0, intid);
}

void send_sgi_int_to_all(uint8_t intid)
{
	set_icc_sgi1r(0, 0, 0, 1, 0, intid);
}
