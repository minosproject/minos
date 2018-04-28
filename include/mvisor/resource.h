#ifndef _MVISOR_RESOURCE_H_
#define _MVISOR_RESOURCE_H_

#include <mvisor/types.h>

enum mvisor_resource_type {
	MVISOR_RESOURCE_TYPE_MEMORY = 0,
	MVISOR_RESOURCE_TYPE_IRQ,
	MVISOR_RESOURCE_TYPE_UNKNOWN,
};

struct memory_resource {
	unsigned long mem_base;
	unsigned long mem_end;
	int type;
	uint32_t vmid;
	char *name;
};

struct irq_resource {
	uint32_t hno;
	uint32_t vno;
	uint32_t vmid;
	uint32_t affinity;
	uint32_t type;
	char *name;
};

int mvisor_parse_resource(void);

#endif
