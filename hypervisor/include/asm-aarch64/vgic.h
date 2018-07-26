#ifndef _MINOS_VGIC_H_
#define _MINOS_VGIC_H_

struct vgic_gicd {
	uint32_t gicd_ctlr;
	uint32_t gicd_typer;
	uint32_t gicd_pidr2;
	uint32_t vmid;
	struct list_head list;
	struct list_head gicr_list;
	spinlock_t gicd_lock;
	unsigned long base;
	unsigned long end;
};

struct vgic_gicr {
	uint32_t gicr_ctlr;
	uint32_t gicr_pidr2;
	uint64_t gicr_typer;
	uint32_t gicr_ispender;
	uint32_t gicr_enabler0;
	uint32_t vcpu_id;
	unsigned long rd_base;
	unsigned long sgi_base;
	unsigned long vlpi_base;
	struct list_head list;
	spinlock_t gicr_lock;
	struct vgic_gicd *gicd;
};

#define GIC_TYPE_GICD		(0x0)
#define GIC_TYPE_GICR_RD	(0x1)
#define GIC_TYPE_GICR_SGI	(0x2)
#define GIC_TYPE_GICR_VLPI	(0x3)
#define GIC_TYPE_INVAILD	(0xff)

void vgic_send_sgi(struct vcpu *vcpu, unsigned long sgi_value);

#endif
