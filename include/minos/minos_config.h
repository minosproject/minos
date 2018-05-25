#ifndef _MINOS_MINOS_CONFIG_H_
#define _MINOS_MINOS_CONFIG_H_

#include <minos/types.h>
#include <config/config.h>

struct vmtag {
	uint32_t vmid;
	char *name;
	char *type;
	int nr_vcpu;
	unsigned long entry;
	int mmu_on;
	unsigned long setup_data;
	int vcpu_affinity[4];
};

struct irqtag {
	uint32_t vno;
	uint32_t hno;
	int enable;
	int owner;
	uint32_t vmid;
	uint32_t affinity;
	unsigned long type;
	char *name;
};

struct memtag {
	unsigned long mem_base;
	unsigned long mem_end;
	int host;
	int sectype;
	int enable;
	int type;
	uint32_t vmid;
	char *name;
};

struct minos_config {
	char *version;
	char *platform;

	size_t nr_vmtag;
	size_t nr_irqtag;
	size_t nr_memtag;

	struct vmtag *vmtags;
	struct irqtag *irqtags;
	struct memtag *memtags;
};

#endif
