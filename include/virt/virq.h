#ifndef __MINOS_VIRQ_H__
#define __MINOS_VIRQ_H__

#include <virt/vm.h>
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

#define VIRQ_AFFINITY_VM_ANY	(0xffff)
#define VIRQ_AFFINITY_VCPU_ANY	(0xff)

#define VM_SGI_VIRQ_NR		(CONFIG_NR_SGI_IRQS)
#define VM_PPI_VIRQ_NR		(CONFIG_NR_PPI_IRQS)
#define VM_LOCAL_VIRQ_NR	(VM_SGI_VIRQ_NR + VM_PPI_VIRQ_NR)

#ifndef CONFIG_HVM_SPI_VIRQ_NR
#define HVM_SPI_VIRQ_NR		(384)
#else
#define HVM_SPI_VIRQ_NR		CONFIG_HVM_SPI_VIRQ_NR
#endif

#define HVM_SPI_VIRQ_BASE	(VM_LOCAL_VIRQ_NR)

#ifndef CONFIG_GVM_SPI_VIRQ_NR
#define GVM_SPI_VIRQ_NR		(64)
#else
#define GVM_SPI_VIRQ_NR		CONFIG_GVM_SPI_VIRQ_NR
#endif

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
#define VIRQS_REQUESTED		(1 << 6)
#define VIRQS_FIQ		(1 << 7)

#define VIRQF_CAN_WAKEUP	(1 << 4)
#define VIRQF_ENABLE		(1 << 5)
#define VIRQF_FIQ		(1 << 7)

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
	uint32_t flags;
	struct list_head list;
};

struct virq_struct {
	uint32_t active_count;
	uint32_t pending_hirq;
	uint32_t pending_virq;
	spinlock_t lock;
	struct list_head pending_list;
	struct list_head active_list;
	struct virq_desc local_desc[VM_LOCAL_VIRQ_NR];
#if defined(CONFIG_VIRQCHIP_VGICV2) || defined(CONFIG_VIRQCHIP_VGICV3)
#define MAX_NR_LRS 64
	struct ffs_table lrs_table;
#endif
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

static int inline virq_is_requested(struct virq_desc *d)
{
	return (d->flags & VIRQS_REQUESTED);
}

static void inline __virq_set_fiq(struct virq_desc *d)
{
	d->flags |= VIRQS_FIQ;
}

static int inline virq_is_fiq(struct virq_desc *d)
{
	return (d->flags & VIRQS_FIQ);
}

static void inline virq_clear_fiq(struct virq_desc *d)
{
	d->flags &= ~VIRQS_FIQ;
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
int virq_can_request(struct vcpu *vcpu, uint32_t virq);
uint32_t get_pending_virq(struct vcpu *vcpu);
int virq_set_fiq(struct vcpu *vcpu, uint32_t virq);

int send_virq_to_vcpu(struct vcpu *vcpu, uint32_t virq);
int send_virq_to_vm(struct vm *vm, uint32_t virq);

int vcpu_has_irq(struct vcpu *vcpu);

int alloc_vm_virq(struct vm *vm);
void release_vm_virq(struct vm *vm, int virq);

int request_virq_affinity(struct vm *vm, uint32_t virq,
		uint32_t hwirq, int affinity, unsigned long flags);
int request_hw_virq(struct vm *vm, uint32_t virq, uint32_t hwirq,
			unsigned long flags);
int request_virq_pervcpu(struct vm *vm, uint32_t virq,
			unsigned long flags);
int request_virq(struct vm *vm, uint32_t virq, unsigned long flags);

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

struct virq_chip *alloc_virq_chip(void);
int virqchip_get_virq_state(struct vcpu *vcpu, struct virq_desc *virq);
void virqchip_send_virq(struct vcpu *vcpu, struct virq_desc *virq);
void virqchip_update_virq(struct vcpu *vcpu,
		struct virq_desc *virq, int action);

#endif
