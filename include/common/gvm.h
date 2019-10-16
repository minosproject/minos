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
 */

#define GVM_VIRT_TIMER_INT	27

#define SP805_IRQ		32
#define SP805_CLK_RATE		100000
#define SP805_IOMEM_BASE	0x1c0f0000
#define SP805_IOMEM_SIZE	0x1000

#define PL031_IRQ		36
#define PL031_IOMEM_BASE	0x1c170000
#define PL031_IOMEM_SIZE	0x1000

#define GVM_IRQ_BASE		64
#define GVM_IRQ_COUNT		32
#define GVM_IRQ_END		(GVM_IRQ_BASE + GVM_IRQ_COUNT)

#define VIRTIO_NET_IRQ		(GVM_IRQ_BASE)
#define VIRTION_NET_IOMEM_BASE	()

#endif
