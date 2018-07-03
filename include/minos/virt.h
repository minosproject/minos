#ifndef __MINOS_VIRT_H_
#define __MINOS_VIRT_H_

#include <minos/arch.h>
#include <minos/types.h>
#include <minos/vcpu.h>

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

struct virqtag {
	uint16_t vno;
	uint16_t hno;
	uint16_t vmid;
	uint16_t vcpu_id;
	uint8_t enable;
	uint8_t hw;
	unsigned int type;
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

struct virt_config {
	char *version;
	char *platform;

	size_t nr_vmtag;
	size_t nr_virqtag;
	size_t nr_memtag;

	struct vmtag *vmtags;
	struct virqtag *virqtags;
	struct memtag *memtags;
};

extern struct virt_config *mv_config;

int taken_from_guest(gp_regs *regs);

void exit_from_guest(struct vcpu *vcpu, gp_regs *regs);
void enter_to_guest(struct vcpu *vcpu, gp_regs *regs);

void save_vcpu_vcpu_state(struct vcpu *vcpu);
void restore_vcpu_vcpu_state(struct vcpu *vcpu);

#endif
