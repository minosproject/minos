#ifndef _MVISOR_RESOURCE_H_
#define _MVISOR_RESOURCE_H_

#include <mvisor/types.h>

enum vmm_resource_type {
	VMM_RESOURCE_TYPE_MEMORY = 0,
	VMM_RESOURCE_TYPE_IRQ,
	VMM_RESOURCE_TYPE_UNKNOWN,
};

struct memory_resource {
	phy_addr_t mem_base;
	phy_addr_t mem_end;
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

int vmm_parse_resource(void);

#endif
