#ifndef _MVISOR_GIC_H_
#define _MVISOR_GIC_H_

#include <core/gicv3.h>

struct vmm_irq;

void gic_init(void);
void gic_secondary_init(void);
uint32_t gic_get_line_num(void);
void gic_mask_irq(struct vmm_irq *vmm_irq);
void gic_eoi_irq(struct vmm_irq *vmm_irq);
void gic_dir_irq(struct vmm_irq *vmm_irq);
uint32_t gic_read_irq(void);
void gic_set_irq_type(struct vmm_irq *vmm_irq, uint32_t type);
void gic_set_irq_priority(struct vmm_irq *vmm_irq, uint32_t pr);
void gic_set_irq_affinity(struct vmm_irq *vmm_irq, int cpu);
void gic_unmask_irq(struct vmm_irq *vmm_irq);

#endif
