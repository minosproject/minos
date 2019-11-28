#ifndef __MINOS_VGIC_H__
#define __MINSO_VGIC_H__

#include <minos/types.h>

struct vcpu;

int vgic_irq_enter_to_guest(struct vcpu *vcpu, void *data);
int vgic_irq_exit_from_guest(struct vcpu *vcpu, void *data);
int vgic_generate_virq(uint32_t *array, int virq);

#endif
