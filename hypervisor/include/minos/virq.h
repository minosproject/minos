#ifndef __MINOS_VIRQ_H__
#define __MINOS_VIRQ_H__

#include <minos/vcpu.h>
#include <minos/cpumask.h>

struct irqtag;

#define VIRQ_STATE_INACTIVE		(0x0)
#define VIRQ_STATE_PENDING		(0x1)
#define VIRQ_STATE_ACTIVE		(0x2)
#define VIRQ_STATE_ACTIVE_AND_PENDING	(0x3)

#define VCPU_MAX_ACTIVE_VIRQS		(64)
#define VIRQ_INVALID_ID			(0xff)

#define VIRQ_ACTION_REMOVE	(0x0)
#define VIRQ_ACTION_ADD		(0x1)
#define VIRQ_ACTION_CLEAR	(0x2)

#define VIRQ_AFFINITY_ANY	(0xffff)

#define VM_SGI_VIRQ_NR		(16)
#define VM_PPI_VIRQ_NR		(16)
#define VM_LOCAL_VIRQ_NR	(VM_SGI_VIRQ_NR + VM_PPI_VIRQ_NR)

#define HVM_SPI_VIRQ_NR		(384)
#define HVM_SPI_VIRQ_BASE	(VM_LOCAL_VIRQ_NR)

#define GVM_SPI_VIRQ_NR		(64)
#define GVM_SPI_VIRQ_BASE	(VM_LOCAL_VIRQ_NR)

#define VIRQ_SPI_OFFSET(virq)	((virq) - VM_LOCAL_VIRQ_NR)
#define VIRQ_SPI_NR(count)	((count) > VM_LOCAL_VIRQ_NR ? VIRQ_SPI_OFFSET((count)) : 0)

#define VM_VIRQ_NR(nr)		((nr) + VM_LOCAL_VIRQ_NR)

#define MAX_HVM_VIRQ		(HVM_SPI_VIRQ_NR + VM_LOCAL_VIRQ_NR)
#define MAX_GVM_VIRQ		(GVM_SPI_VIRQ_NR + VM_LOCAL_VIRQ_NR)

#define VIRQ_FLAGS_CAN_WAKEUP	(1 << 0)

enum virq_domain_type {
	VIRQ_DOMAIN_SGI = 0,
	VIRQ_DOMAIN_PPI,
	VIRQ_DOMAIN_SPI,
	VIRQ_DOMAIN_LPI,
	VIRQ_DOMAIN_MAX,
};

/* virq_desc use 32 bytes */
struct virq_desc {
	uint8_t id;
	uint8_t state;
	uint8_t hw;
	uint8_t enable;
	uint8_t pr;
	uint8_t vcpu_id;
	uint8_t type;
	uint8_t flags;
	uint16_t vmid;
	uint16_t vno;
	uint16_t hno;
	uint16_t padding;
	struct list_head list;
} __packed__;

struct virq_struct {
	int active_virqs;
	uint32_t active_count;
	uint32_t pending_hirq;
	uint32_t pending_virq;
	spinlock_t lock;
	struct list_head pending_list;
	struct list_head active_list;
	DECLARE_BITMAP(irq_bitmap, VCPU_MAX_ACTIVE_VIRQS);
	struct virq_desc local_desc[VM_LOCAL_VIRQ_NR];
};

int virq_enable(struct vcpu *vcpu, uint32_t virq);
int virq_disable(struct vcpu *vcpu, uint32_t virq);
void vcpu_virq_struct_init(struct vcpu *vcpu);
void vcpu_virq_struct_reset(struct vcpu *vcpu);

void vm_virq_reset(struct vm *vm);
void send_vsgi(struct vcpu *sender,
		uint32_t sgi, cpumask_t *cpumask);
void clear_pending_virq(struct vcpu *vcpu, uint32_t irq);
int virq_set_priority(struct vcpu *vcpu, uint32_t virq, int pr);
int virq_set_type(struct vcpu *vcpu, uint32_t virq, int value);
uint32_t virq_get_type(struct vcpu *vcpu, uint32_t virq);

int send_virq_to_vcpu(struct vcpu *vcpu, uint32_t virq);
int send_virq_to_vm(struct vm *vm, uint32_t virq);

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

int alloc_vm_virq(struct vm *vm);
void release_vm_virq(struct vm *vm, int virq);

static inline int alloc_hvm_virq(void)
{
	return alloc_vm_virq(get_vm_by_id(0));
}

static inline int alloc_gvm_virq(struct vm *vm)
{
	return alloc_vm_virq(vm);
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
