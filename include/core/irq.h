#ifndef _MVISOR_IRQ_H_
#define _MVISOR_IRQ_H_

#include <core/types.h>
#include <core/gic.h>

#define MAX_IRQ_NAME_SIZE	32

struct vmm_irq {
	uint32_t hno;
	uint32_t vno;
	uint32_t vmid;
	uint32_t affinity_vcpu;
	uint32_t affinity_pcpu;
	uint32_t flags;
	char name[MAX_IRQ_NAME_SIZE];
};

enum sgi_mode {
	SGI_TO_OTHERS,
	SGI_TO_SELF,
	SGI_TO_LIST,
};

#define SPI_OFFSET(num)	(num - 32)

#define IRQ_TYPE_NONE           		(0x00000000)
#define IRQ_TYPE_EDGE_RISING    		(0x00000001)
#define IRQ_TYPE_EDGE_FALLING  			(0x00000002)
#define IRQ_TYPE_LEVEL_HIGH     		(0x00000004)
#define IRQ_TYPE_LEVEL_LOW      		(0x00000008)
#define IRQ_TYPE_SENSE_MASK     		(0x0000000f)
#define IRQ_TYPE_INVALID        		(0x00000010)
#define IRQ_TYPE_EDGE_BOTH \
    (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
#define IRQ_TYPE_LEVEL_MASK \
    (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH)
#define IRQ_TYPE_MASK				(0x000000ff)

#define IRQ_STATUS_MASKED			(0x00000000)
#define IRQ_STATUS_UNMASKED			(0x00000100)
#define IRQ_STATUS_MASK				(0x00000f00)

void enable_irq(void);
void disable_irq(void);

#endif
