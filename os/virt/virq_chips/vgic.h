#ifndef __MINOS_VGIC_H__
#define __MINSO_VGIC_H__

#include <minos/types.h>

struct vcpu;

int vgic_irq_enter_to_guest(struct vcpu *vcpu, void *data);
int vgic_irq_exit_from_guest(struct vcpu *vcpu, void *data);
int gic_vm0_virq_data(uint32_t *array, int vspi_nr, int type);

#endif
