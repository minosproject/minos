#ifndef __MINOS_VIRQ_H__
#define __MINOS_VIRQ_H__

#include <minos/vcpu.h>
#include <minos/cpumask.h>
#include <config/config.h>

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

#ifndef CONFIG_HVM_SPI_VIRQ_NR
#define HVM_SPI_VIRQ_NR		(384)
#else
#define HVM_SPI_VIRQ_NR		CONFIG_HVM_SPI_VIRQ_NR
#endif

#define HVM_SPI_VIRQ_BASE	(VM_LOCAL_VIRQ_NR)

#define GVM_SPI_VIRQ_NR		(64)
#define GVM_SPI_VIRQ_BASE	(VM_LOCAL_VIRQ_NR)

#define VIRQ_SPI_OFFSET(virq)	((virq) - VM_LOCAL_VIRQ_NR)
#define VIRQ_SPI_NR(count)	((count) > VM_LOCAL_VIRQ_NR ? VIRQ_SPI_OFFSET((count)) : 0)

#define VM_VIRQ_NR(nr)		((nr) + VM_LOCAL_VIRQ_NR)

#define MAX_HVM_VIRQ		(HVM_SPI_VIRQ_NR + VM_LOCAL_VIRQ_NR)
#define MAX_GVM_VIRQ		(GVM_SPI_VIRQ_NR + VM_LOCAL_VIRQ_NR)

#define VIRQS_PENDING		(1 << 0)
#define VIRQS_ENABLED		(1 << 1)
#define VIRQS_SUSPEND		(1 << 2)
#define VIRQS_HW		(1 << 3)
#define VIRQS_CAN_WAKEUP	(1 << 4)

enum virq_domain_type {
	VIRQ_DOMAIN_SGI = 0,
	VIRQ_DOMAIN_PPI,
	VIRQ_DOMAIN_SPI,
	VIRQ_DOMAIN_LPI,
	VIRQ_DOMAIN_MAX,
};

struct virq_desc {
	uint8_t id;
	uint8_t state;
	uint8_t pr;
	uint8_t src;
	uint8_t type;
	uint8_t vcpu_id;
	uint16_t vmid;
	uint16_t vno;
	uint16_t hno;
	unsigned long flags;
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

static void inline virq_set_enable(struct virq_desc *d)
{
	d->flags |= VIRQS_ENABLED;
}

static void inline virq_clear_enable(struct virq_desc *d)
{
	d->flags &= ~VIRQS_ENABLED;
}

static int inline virq_is_enabled(struct virq_desc *d)
{
	return (d->flags & VIRQS_ENABLED);
}

static void inline virq_set_wakeup(struct virq_desc *d)
{
	d->flags |= VIRQS_CAN_WAKEUP;
}

static void inline virq_clear_wakeup(struct virq_desc *d)
{
	d->flags &= ~VIRQS_CAN_WAKEUP;
}

static int inline virq_can_wakeup(struct virq_desc *d)
{
	return (d->flags & VIRQS_CAN_WAKEUP);
}

static void inline virq_set_suspend(struct virq_desc *d)
{
	d->flags |= VIRQS_SUSPEND;
}

static void inline virq_clear_suspend(struct virq_desc *d)
{
	d->flags &= ~VIRQS_SUSPEND;
}

static int inline virq_is_suspend(struct virq_desc *d)
{
	return (d->flags & VIRQS_SUSPEND);
}

static void inline virq_set_hw(struct virq_desc *d)
{
	d->flags |= VIRQS_HW;
}

static void inline virq_clear_hw(struct virq_desc *d)
{
	d->flags &= ~VIRQS_HW;
}

static int inline virq_is_hw(struct virq_desc *d)
{
	return (d->flags & VIRQS_HW);
}

static void inline virq_set_pending(struct virq_desc *d)
{
	d->flags |= VIRQS_PENDING;
}

static void inline virq_clear_pending(struct virq_desc *d)
{
	d->flags &= ~VIRQS_PENDING;
}

static int inline virq_is_pending(struct virq_desc *d)
{
	return (d->flags & VIRQS_PENDING);
}

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
uint32_t virq_get_affinity(struct vcpu *vcpu, uint32_t virq);
uint32_t virq_get_pr(struct vcpu *vcpu, uint32_t virq);
uint32_t virq_get_state(struct vcpu *vcpu, uint32_t virq);

int send_virq_to_vcpu(struct vcpu *vcpu, uint32_t virq);
int send_virq_to_vm(struct vm *vm, uint32_t virq);

int vcpu_has_irq(struct vcpu *vcpu);

int alloc_vm_virq(struct vm *vm);
void release_vm_virq(struct vm *vm, int virq);
int virq_unmask_and_init(struct vm *vm, uint32_t virq);
uint32_t get_pending_virq(struct vcpu *vcpu);

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
