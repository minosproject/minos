#ifndef _MINOS_VGICV3_H_
#define _MINOS_VGICV3_H_

#include <minos/smp.h>
#include <minos/vdev.h>

struct vgic_gicd {
	uint32_t gicd_ctlr;
	uint32_t gicd_typer;
	uint32_t gicd_pidr2;
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
};

struct vgicv3_dev {
	struct vdev vdev;
	struct vgic_gicd gicd;
	struct vgic_gicr *gicr[NR_CPUS];
};

#define GIC_TYPE_GICD		(0x0)
#define GIC_TYPE_GICR_RD	(0x1)
#define GIC_TYPE_GICR_SGI	(0x2)
#define GIC_TYPE_GICR_VLPI	(0x3)
#define GIC_TYPE_INVAILD	(0xff)

#define GVM_VGIC_IOMEM_BASE	(0x2f000000)
#define GVM_VGIC_IOMEM_SIZE	(0x200000)

#define GVM_VGICD_IOMEM_BASE	(GVM_VGIC_IOMEM_BASE)
#define GVM_VGICD_IOMEM_SIZE	(0x10000)

#define GVM_VGICR_IOMEM_BASE	(0x2f100000)

void vgicv3_send_sgi(struct vcpu *vcpu, unsigned long sgi_value);

#endif
