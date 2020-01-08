#ifndef __VIRQ_CHIP_H__
#define __VIRQ_CHIP_H__

#include <minos/types.h>
#include <minos/device_id.h>

struct vcpu;

#define VIRQCHIP_F_HW_VIRT	(1 << 0)

struct virq_chip {
	int (*exit_from_guest)(struct vcpu *vcpu, void *data);
	int (*enter_to_guest)(struct vcpu *vcpu, void *data);
	int (*generate_virq)(uint32_t *array, int virq);
	int (*xlate)(struct device_node *node, uint32_t *intspec,
			unsigned int intsize, uint32_t *hwirq,
			unsigned long *type);
	int (*send_virq)(struct vcpu *vcpu, struct virq_desc *virq);
	int (*get_virq_state)(struct vcpu *vcpu, struct virq_desc *virq);
	int (*update_virq)(struct vcpu *vcpu, struct virq_desc *virq, int action);
	int (*vcpu_init)(struct vcpu *vcpu, void *pdata, unsigned long flags);

	void *inc_pdata;
	unsigned long flags;
};

struct virq_chip *alloc_virq_chip(void);
int virqchip_get_virq_state(struct vcpu *vcpu, struct virq_desc *virq);
void virqchip_send_virq(struct vcpu *vcpu, struct virq_desc *virq);
void virqchip_update_virq(struct vcpu *vcpu,
		struct virq_desc *virq, int action);

#endif
