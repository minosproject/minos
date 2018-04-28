#ifndef _MVISOR_IRQ_H_
#define _MVISOR_IRQ_H_

#include <mvisor/types.h>
#include <asm/asm_irq.h>
#include <mvisor/device_id.h>
#include <mvisor/init.h>
#include <config/config.h>
#include <mvisor/smp.h>
#include <mvisor/spinlock.h>
#include <mvisor/cpumask.h>
#include <mvisor/resource.h>

struct vcpu;

#define MAX_IRQ_NAME_SIZE	32
#define BAD_IRQ			(1024)

#define IRQ_FLAG_TYPE_NONE           		(0x00000000)
#define IRQ_FLAG_TYPE_EDGE_RISING    		(0x00000001)
#define IRQ_FLAG_TYPE_EDGE_FALLING  		(0x00000002)
#define IRQ_FLAG_TYPE_LEVEL_HIGH     		(0x00000004)
#define IRQ_FLAG_TYPE_LEVEL_LOW      		(0x00000008)
#define IRQ_FLAG_TYPE_SENSE_MASK     		(0x0000000f)
#define IRQ_FLAG_TYPE_INVALID        		(0x00000010)
#define IRQ_FLAG_TYPE_EDGE_BOTH \
    (IRQ_FLAG_TYPE_EDGE_FALLING | IRQ_FLAG_TYPE_EDGE_RISING)
#define IRQ_FLAG_TYPE_LEVEL_BOTH \
    (IRQ_FLAG_TYPE_LEVEL_LOW | IRQ_FLAG_TYPE_LEVEL_HIGH)
#define IRQ_FLAG_TYPE_MASK			(0x000000ff)

#define IRQ_FLAG_STATUS_MASKED			(0x00000000)
#define IRQ_FLAG_STATUS_UNMASKED		(0x00000100)
#define IRQ_FLAG_STATUS_MASK			(0x00000f00)

#define IRQ_FLAG_OWNER_GUEST			(0x00000000)
#define IRQ_FLAG_OWNER_MVISOR			(0x00001000)
#define IRQ_FLAG_OWNER_MASK			(0x0000f000)

#define IRQ_FLAG_AFFINITY_VCPU			(0x00010000)
#define IRQ_FLAG_AFFINITY_PERCPU		(0x00000000)
#define IRQ_FLAG_AFFINITY_MASK			(0x000f0000)

typedef enum sgi_mode {
	SGI_TO_LIST = 0,
	SGI_TO_OTHERS,
	SGI_TO_SELF,
} sgi_mode_t;

enum irq_type {
	IRQ_TYPE_SGI = 0,
	IRQ_TYPE_PPI,
	IRQ_TYPE_SPI,
	IRQ_TYPE_LPI,
	IRQ_TYPE_SPECIAL,
	IRQ_TYPE_BAD,
};

struct irq_desc;
typedef int (*irq_handle_t)(uint32_t irq, void *data);

#define CONFIG_VCPU_MAX_ACTIVE_IRQS	(16)

#define VIRQ_STATE_INACTIVE		(0x0)
#define VIRQ_STATE_PENDING		(0x1)
#define VIRQ_STATE_ACTIVE		(0x2)
#define VIRQ_STATE_ACTIVE_AND_PENDING	(0x3)
#define VIRQ_STATE_OFFLINE		(0x4)

struct virq {
	uint32_t h_intno;
	uint32_t v_intno;
	int hw;
	int state;
	int id;
	struct list_head list;
};

struct irq_struct {
	uint32_t active_count;
	uint32_t pending_count;
	spinlock_t lock;
	struct list_head pending_list;
	DECLARE_BITMAP(irq_bitmap, CONFIG_VCPU_MAX_ACTIVE_IRQS);
	struct virq virqs[CONFIG_VCPU_MAX_ACTIVE_IRQS];
};

#define VIRQ_ACTION_REMOVE	(0x0)
#define VIRQ_ACTION_ADD		(0x1)

struct irq_chip {
	uint32_t (*get_pending_irq)(void);
	void (*irq_mask)(uint32_t irq);
	void (*irq_unmask)(uint32_t irq);
	void (*irq_eoi)(uint32_t irq);
	void (*irq_dir)(uint32_t irq);
	int (*irq_set_affinity)(uint32_t irq, uint32_t pcpu);
	int (*irq_set_type)(uint32_t irq, unsigned int flow_type);
	int (*irq_set_priority)(uint32_t irq, uint32_t pr);
	void (*send_sgi)(uint32_t irq, enum sgi_mode mode, cpumask_t *mask);
	int (*send_virq)(struct virq *virq);
	int (*get_virq_state)(struct virq *virq);
	int (*update_virq)(struct virq *virq, int action);
	int (*init)(void);
	int (*secondary_init)(void);
};

/*
 * if a irq is handled by mvisor, then need to register
 * the irq handler otherwise it will return the vnum
 * to the handler and pass the virq to the vm
 */
struct irq_desc {
	uint32_t hno;
	uint32_t vno;
	uint32_t vmid;
	uint32_t affinity_vcpu;
	uint32_t affinity_pcpu;
	uint32_t flags;
	spinlock_t lock;
	char name[MAX_IRQ_NAME_SIZE];
	unsigned long irq_count;
	irq_handle_t handler;
	void *pdata;
};

enum irq_domain_type {
	IRQ_DOMAIN_SPI = 0,
	IRQ_DOMAIN_LOCAL,
	IRQ_DOMAIN_MAX,
};

struct irq_domain;
struct irq_domain_ops {
	struct irq_desc **(*alloc_irqs)(uint32_t s, uint32_t c);
	int (*register_irq)(struct irq_domain *domain, struct irq_resource *res);
	struct irq_desc *(*get_irq_desc)(struct irq_domain *d, uint32_t irq);
	uint32_t (*virq_to_irq)(struct irq_domain *d, uint32_t virq);
	void (*setup_irqs)(struct irq_domain *d);
	int (*irq_handler)(struct irq_domain *d, struct irq_desc *irq);
};

struct irq_domain {
	uint32_t start;
	uint32_t count;
	struct irq_desc **irqs;
	struct irq_domain_ops *ops;
};

#define enable_local_irq() arch_enable_local_irq()
#define disable_local_irq() arch_disable_local_irq()

int mvisor_irq_init(void);
int irq_desc_secondary_init(void);
int mvisor_register_irq_entry(void *res);
void mvisor_setup_irqs(void);
int do_irq_handler(void);
int request_irq(uint32_t irq, irq_handle_t handler, void *data);
void vcpu_irq_struct_init(struct irq_struct *irq_struct);
int irq_add_spi(uint32_t start, uint32_t cnt);
int irq_add_local(uint32_t start, uint32_t cnt);

void __virq_enable(uint32_t virq, int enable);
void __irq_enable(uint32_t irq, int enable);
int send_virq_hw(uint32_t vmid, uint32_t virq, uint32_t hirq);
int send_virq(uint32_t vmid, uint32_t virq);
void send_vsgi(struct vcpu *sender,
		uint32_t sgi, cpumask_t *cpumask);
int vcpu_has_virq_pending(struct vcpu *vcpu);
int vcpu_has_virq_active(struct vcpu *vcpu);
int vcpu_has_virq(struct vcpu *vcpu);
void clear_pending_virq(uint32_t irq);

static inline void virq_mask(uint32_t virq)
{
	__virq_enable(virq, 0);
}

static inline void virq_unmask(uint32_t virq)
{
	__virq_enable(virq, 1);
}

static inline void irq_unmask(uint32_t irq)
{
	__irq_enable(irq, 1);
}

static inline void irq_mask(uint32_t irq)
{
	__irq_enable(irq, 0);
}

#endif
