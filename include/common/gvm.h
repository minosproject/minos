#ifndef __MINOS_GVM_H__
#define __MINOS_GVM_H__


/*
 * each guest vm may have at least 64 spi irqs,
 * the 32 - 63 virqs is for host vdevs and the
 * 64 - 95 virqs is for guest vms. also the hypervisor
 * provide auto allocation API to allocate virqs.
 * the only case to use the allocate API is to allocate
 * the vmcs irq after all the virtual devices has been
 * created
 *
 * the memory map for a guest vm showed as below:
 * 0x00000000 -> 0x1fffffff [IOMEM for device]
 *    0x00000000 -> 0x0fffffff [Map to directly io device passthough]
 *    0x10000000 -> 0x1fffffff [virtual device]
 * 0x20000000 -> 0xffffffff [Normal memory]
 */

#define GVM_VIRT_TIMER_INT	27

#define GIC_IRQ				9

#define GICV2_GICD_IOMEM_BASE		0x10000000
#define GICV2_GICD_IOMEM_SIZE		0x10000
#define GICV2_GICC_IOMEM_BASE		0x10010000
#define GICV2_GICC_IOMEM_SIZE		0x2000
#define GICV2_GICH_IOMEM_BASE		0x10012000
#define GICV2_GICH_IOMEM_SIZE		0x2000
#define GICV2_GICV_IOMEM_BASE		0x10014000
#define GICV2_GICV_IOMEM_SIZE		0x2000

#define GICV3_GICD_IOMEM_BASE		0x10000000
#define GICV3_GICD_IOMEM_SIZE		0x10000
#define GICV3_GICR_IOMEM_BASE		0x10200000
#define GICV3_GICR_IOMEM_SIZE		0x200000
#define GICV3_GICC_IOMEM_BASE		0x10400000
#define GICV3_GICC_IOMEM_SIZE		0x2000
#define GICV3_GICH_IOMEM_BASE		0x10410000
#define GICV3_GICH_IOMEM_SIZE		0x2000
#define GICV3_GICV_IOMEM_BASE		0x10420000
#define GICV3_GICV_IOMEM_SIZE		0x2000
#define GICV3_ITS_IOMEM_BASE		0x10430000
#define GICV3_ITS_IOMEM_SIZE		0x2000

#define SP805_IRQ			32
#define SP805_CLK_RATE			100000
#define SP805_IOMEM_BASE		0x10440000
#define SP805_IOMEM_SIZE		0x1000

#define PL031_IRQ			33
#define PL031_IOMEM_BASE		0x10441000
#define PL031_IOMEM_SIZE		0x1000

#define GVM_IRQ_BASE			64
#define GVM_IRQ_COUNT			32
#define GVM_IRQ_END			(GVM_IRQ_BASE + GVM_IRQ_COUNT)

#define VM_MAX_VIRTIO_DEVICES		32
#define VM_VIRTIO_IOMEM_BASE		0x1fe00000
#define VM_VIRTIO_IOMEM_SIZE		(0x1000 * VM_MAX_VIRTIO_DEVICES)

#endif
