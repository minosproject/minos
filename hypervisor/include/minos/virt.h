#ifndef __MINOS_VIRT_H_
#define __MINOS_VIRT_H_

#include <minos/minos.h>
#include <minos/arch.h>
#include <minos/vcpu.h>
#include <minos/vmodule.h>

struct vmtag {
	uint32_t vmid;
	char *name;
	char *type;
	int nr_vcpu;
	unsigned long entry;
	int bit64;
	unsigned long setup_data;
	int vcpu_affinity[4];
};

struct irqtag {
	uint16_t vno;
	uint16_t hno;
	uint16_t vmid;
	uint16_t vcpu_id;
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
	size_t nr_irqtag;
	size_t nr_memtag;

	struct vmtag *vmtags;
	struct irqtag *irqtags;
	struct memtag *memtags;
};

extern struct virt_config *mv_config;

static inline int taken_from_guest(gp_regs *regs)
{
	return arch_taken_from_guest(regs);
}

static inline void exit_from_guest(struct vcpu *vcpu, gp_regs *regs)
{
	do_hooks((void *)vcpu, (void *)regs,
			MINOS_HOOK_TYPE_EXIT_FROM_GUEST);
}

static inline void enter_to_guest(struct vcpu *vcpu, gp_regs *regs)
{
	do_hooks((void *)vcpu, (void *)regs,
			MINOS_HOOK_TYPE_ENTER_TO_GUEST);
}

static inline void save_vcpu_vcpu_state(struct vcpu *vcpu)
{
	save_vcpu_vmodule_state(vcpu);
}

static inline void restore_vcpu_vcpu_state(struct vcpu *vcpu)
{
	restore_vcpu_vmodule_state(vcpu);
}

#endif
