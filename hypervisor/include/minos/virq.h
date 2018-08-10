#ifndef __MINOS_VIRQ_H__
#define __MINOS_VIRQ_H__

#include <minos/vcpu.h>
#include <minos/cpumask.h>

struct virqtag;

#define VIRQ_STATE_INACTIVE		(0x0)
#define VIRQ_STATE_PENDING		(0x1)
#define VIRQ_STATE_ACTIVE		(0x2)
#define VIRQ_STATE_ACTIVE_AND_PENDING	(0x3)
#define VIRQ_STATE_OFFLINE		(0x4)

#define VIRQ_ACTION_REMOVE	(0x0)
#define VIRQ_ACTION_ADD		(0x1)
#define VIRQ_ACTION_CLEAR	(0x2)

#define VIRQ_AFFINITY_ANY	(0xffff)

#define MAX_HVM_VIRQ		(512)
#define HVM_VIRQ_BASE		(256)

#define MAX_GVM_VIRQ		(128)
#define GVM_VIRQ_BASE		(256)

struct virq {
	uint32_t h_intno;
	uint32_t v_intno;
	uint8_t hw;
	uint8_t state;
	uint16_t id;
	uint16_t pr;
	struct list_head list;
};

struct virq_struct {
	uint32_t active_count;
	uint32_t pending_hirq;
	uint32_t pending_virq;
	spinlock_t lock;
	struct list_head pending_list;
	struct list_head active_list;
	DECLARE_BITMAP(irq_bitmap, CONFIG_VCPU_MAX_ACTIVE_IRQS);
	DECLARE_BITMAP(local_irq_mask, VCPU_MAX_LOCAL_IRQS);
	struct virq virqs[CONFIG_VCPU_MAX_ACTIVE_IRQS];
};

int virq_enable(uint32_t virq, int enable);
void vcpu_virq_struct_init(struct virq_struct *irq_struct);

int send_virq_hw(uint32_t vmid, uint32_t virq, uint32_t hirq);
int send_virq_to_vcpu(struct vcpu *vcpu, uint32_t virq);
void send_vsgi(struct vcpu *sender,
		uint32_t sgi, cpumask_t *cpumask);
void clear_pending_virq(uint32_t irq);
int register_virq(struct virqtag *v);
int virq_set_priority(uint32_t virq, int pr);
int alloc_virtual_irqs(uint32_t start, uint32_t count, int type);

static inline int send_virq_to_vm(uint32_t vmid, uint32_t virq)
{
	/*
	 * default all the virq do not attached to
	 * the hardware irq will send to the vcpu0
	 * of a vm
	 */
	return send_virq_to_vcpu(get_vcpu_by_id(vmid, 0), virq);
}

static inline int vcpu_has_virq_pending(struct vcpu *vcpu)
{
	return (!!vcpu->virq_struct->pending_virq);
}

static inline int vcpu_has_hwirq_pending(struct vcpu *vcpu)
{
	return (!!vcpu->virq_struct->pending_hirq);
}

static inline int vcpu_has_irq(struct vcpu *vcpu)
{
	return ((vcpu->virq_struct->pending_hirq +
			vcpu->virq_struct->pending_virq));
}

static inline void virq_mask(uint32_t virq)
{
	virq_enable(virq, 0);
}

static inline void virq_unmask(uint32_t virq)
{
	virq_enable(virq, 1);
}

int alloc_vm_virq(struct vm *vm, int count);
void release_vm_virq(struct vm *vm, int virq);

static inline int alloc_hvm_virq(void)
{
	return alloc_vm_virq(get_vm_by_id(0), MAX_HVM_VIRQ);
}

static inline int alloc_gvm_virq(struct vm *vm)
{
	return alloc_vm_virq(vm, MAX_GVM_VIRQ);
}

static void inline release_hvm_virq(int virq)
{
	return release_vm_virq(get_vm_by_id(0), virq);
}

static void inline release_gvm_virq(struct vm *vm, int virq)
{
	return release_vm_virq(vm, virq);
}

#endif
