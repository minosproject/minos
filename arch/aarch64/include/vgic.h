#ifndef _MVISOR_VGIC_H_
#define _MVISOR_VGIC_H_

void vgic_send_sgi(vcpu_t *vcpu, unsigned long sgi_value);

#endif
