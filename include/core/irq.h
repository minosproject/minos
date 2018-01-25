#ifndef _MVISOR_IRQ_H_
#define _MVISOR_IRQ_H_

#include <core/types.h>

#define MAX_IRQ_NAME_SIZE	32

struct vmm_irq {
	uint32_t hno;
	uint32_t vno;
	uint32_t vmid;
	uint32_t affinity_vcpu;
	uint32_t affinity_pcpu;
	char name[MAX_IRQ_NAME_SIZE];
};

#define SPI_OFFSET(num)	(num - 32)

#endif
