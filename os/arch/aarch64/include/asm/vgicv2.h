#ifndef _MINOS_VGICV2_H_
#define _MINOS_VGICV2_H_

#include <minos/smp.h>
#include <virt/vdev.h>

#define VGICV2_GICD_GVM_BASE	(0x2f000000)
#define VGICV2_GICD_GVM_SIZE	(0x10000)
#define VGICV2_GICC_GVM_BASE	(0x2c000000)
#define VGICV2_GICC_GVM_SIZE	(0x2000)

#endif
