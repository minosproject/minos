#ifndef _MVISOR_IRQ_H_
#define _MVISOR_IRQ_H_

#include <mvisor/types.h>
#include <asm/asm_irq.h>
#include <mvisor/device_id.h>
#include <mvisor/init.h>
#include <config/config.h>
#include <mvisor/smp.h>
#include <mvisor/vcpu.h>

#define MAX_IRQ_NAME_SIZE	32

#define IRQ_FLAG_TYPE_NONE           		(0x00000000)
#define IRQ_FLAG_TYPE_EDGE_RISING    		(0x00000001)
#define IRQ_FLAG_TYPE_EDGE_FALLING  		(0x00000002)
#define IRQ_FLAG_TYPE_LEVEL_HIGH     		(0x00000004)
#define IRQ_FLAG_TYPE_LEVEL_LOW      		(0x00000008)
#define IRQ_FLAG_TYPE_SENSE_MASK     		(0x0000000f)
#define IRQ_FLAG_TYPE_INVALID        		(0x00000010)
#define IRQ_FLAG_TYPE_EDGE_BOTH \
    (IRQ_FLAG_TYPE_EDGE_FALLING | IRQ_FLAG_TYPE_EDGE_RISING)
#define IRQ_FLAG_TYPE_LEVEL_MASK \
    (IRQ_FLAG_TYPE_LEVEL_LOW | IRQ_FLAG_TYPE_LEVEL_HIGH)
#define IRQ_FLAG_TYPE_MASK			(0x000000ff)

#define IRQ_FLAG_STATUS_MASKED			(0x00000000)
#define IRQ_FLAG_STATUS_UNMASKED		(0x00000100)
#define IRQ_FLAG_STATUS_MASK			(0x00000f00)

#define IRQ_FLAG_OWNER_GUEST			(0x00000000)
#define IRQ_FLAG_OWNER_VMM			(0x00001000)
#define IRQ_FLAG_OWNER_MASK			(0x0000f000)

enum sgi_mode {
	SGI_TO_OTHERS,
	SGI_TO_SELF,
	SGI_TO_LIST,
};

enum irq_type {
	IRQ_TYPE_SGI = 0,
	IRQ_TYPE_PPI,
	IRQ_TYPE_SPI,
	IRQ_TYPE_LPI,
	IRQ_TYPE_SPECIAL,
	IRQ_TYPE_BAD,
};

struct vmm_irq;

struct irq_chip {
	uint32_t irq_start;
	uint32_t irq_num;
	void (*irq_mask)(struct vmm_irq *data);
	void (*irq_unmask)(struct vmm_irq *data);
	void (*irq_eoi)(struct vmm_irq *data);
	int (*irq_set_affinity)(struct vmm_irq *data, cpumask_t *dest);
	int (*irq_set_type)(struct vmm_irq *data, unsigned int flow_type);
	int (*irq_set_priority)(struct vmm_irq *data, uint32_t pr);
	uint32_t (*get_pending_irq)(void);
	int (*get_irq_type)(uint32_t irq);
	void (*send_sgi)(struct vmm_irq *data, enum sgi_mode mode, cpumask_t *mask);
	int (*handle_sgi_int)(uint32_t irq, vcpu_t *vcpu);
	int (*handle_ppi_int)(uint32_t irq, vcpu_t *vcpu);
	int (*handle_spi_int)(uint32_t irq, vcpu_t *vcpu);
	int (*handle_lpi_int)(uint32_t irq, vcpu_t *vcpu);
	int (*handle_special_int)(uint32_t irq, vcpu_t *vcpu);
	int (*handle_bad_int)(uint32_t irq, vcpu_t *vcpu);
	int (*init)(void);
	int (*secondary_init)(void);
};

/*
 * if a irq is handled by vmm, then need to register
 * the irq handler otherwise it will return the vnum
 * to the handler and pass the virq to the vm
 */
struct vmm_irq {
	uint32_t hno;
	uint32_t vno;
	uint32_t vmid;
	uint32_t affinity_vcpu;
	uint32_t affinity_pcpu;
	uint32_t flags;
	char name[MAX_IRQ_NAME_SIZE];
	unsigned long irq_count;
	int (*irq_handler)(uint32_t irq, void *data);
	void *pdata;
};

#define enable_local_irq() arch_enable_local_irq()
#define disable_local_irq() arch_disable_local_irq()

int vmm_alloc_irqs(uint32_t start,
		uint32_t end, unsigned long flags);
int register_irq_chip(struct irq_chip *chip);

int vmm_irq_init(void);

int vmm_irq_secondary_init(void);

int vmm_register_irq_entry(void *res);

void vmm_setup_irqs(void);

#endif
