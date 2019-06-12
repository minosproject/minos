#ifndef __VIRQ_CHIP_H__
#define __VIRQ_CHIP_H__

#include <minos/types.h>

struct vcpu;

#define VIRQCHIP_F_HW_VIRT	(1 << 0)

struct virq_chip {
	int (*exit_from_guest)(struct vcpu *vcpu, void *data);
	int (*enter_to_guest)(struct vcpu *vcpu, void *data);
	int (*vm0_virq_data)(uint32_t *array, int vspi_nr, int type);
	int (*xlate)(struct device_node *node, uint32_t *intspec,
			unsigned int intsize, uint32_t *hwirq,
			unsigned long *type);
	int (*send_virq)(struct vcpu *vcpu, struct virq_desc *virq);
	int (*get_virq_state)(struct vcpu *vcpu, struct virq_desc *virq);
	int (*update_virq)(struct vcpu *vcpu, struct virq_desc *virq, int action);

	/* for vgicv2 and vgicv3 that support hw virtualaztion */
#if defined(CONFIG_VIRQCHIP_VGICV2) || defined(CONFIG_VIRQCHIP_VGICV3)
#define MAX_NR_LRS 64
	int nr_lrs;
	DECLARE_BITMAP(irq_bitmap, MAX_NR_LRS);
#endif

	void *inc_pdata;
	unsigned long flags;
};

struct virq_chip *alloc_virq_chip(void);
int virqchip_get_virq_state(struct vcpu *vcpu, struct virq_desc *virq);
void virqchip_send_virq(struct vcpu *vcpu, struct virq_desc *virq);
void virqchip_update_virq(struct vcpu *vcpu,
		struct virq_desc *virq, int action);

#endif
