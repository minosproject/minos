#ifndef _MINOS_GICV3_H_
#define _MINOS_GICV3_H_

#include <asm/gic_reg.h>

#if 0
#define GICD_CTLR_ENABLE_GRP0		(1 << 0)
#define GICD_CTLR_ENABLE_GRP1NS		(1 << 1)
#define GICD_CTLR_ENABLE_GRP1A		(1 << 1)
#define GICD_CTLR_ENABLE_GRP1S		(1 << 2)
#define GICD_CTLR_ENABLE_ALL		((1 << 0) | (1 << 1) | (1 << 2))
#define GICD_CTLR_ARE_S			(1 << 4)
#define GICD_CTLR_ARE_NS		(1 << 5)
#define GICD_CTLR_DS			(1 << 6)
#define GICD_CTLR_E1NWF			(1 << 7)
#else /* for no-secure access */
#define GICD_CTLR_ENABLE_GRP1		(0 << 1)
#define GICD_CTLR_ENABLE_GRP1A		(1 << 1)
#define GICD_CTLR_ARE_NS		(1 << 4)
#endif

#define GICD_IROUTER_MODE_SPECIFIC	(0)
#define GICD_IROUTER_MODE_ANY		(1 << 31)

#define GICD_ICFGR_LEVEL		(0)
#define GICD_ICFGR_EDGE			(1 << 31)

#define GICD_IGROUPR_G0S		(0)
#define GICD_IGROUPR_G1NS		(1 << 0)
#define GICD_IGROUPR_G1S		(1 << 2)

#define GICR_WAKER_PROCESSOR_SLEEP	(1 << 1)
#define GICR_WAKER_CHILDREN_ASLEEP	(1 << 2)

#define GICD_CTLR			(0x0000)
#define GICD_TYPER			(0x0004)
#define GICD_IIDR			(0x0008)
#define GICD_STATUSR			(0x0010)
#define GICD_SETSPI_NSR			(0x0040)
#define GICD_CLRSPI_NSR			(0x0048)
#define GICD_SETSPI_SR			(0x0050)
#define GICD_CLRSPI_SR			(0x0058)
#define GICD_SEIR			(0x0068)
#define GICD_IGROUPR			(0x0080)
#define GICD_ISENABLER			(0x0100)
#define GICD_ISENABLER_END		(0x017c - 1)
#define GICD_ICENABLER			(0x0180)
#define GICD_ICENABLER_END		(0x01fc - 1)
#define GICD_ISPENDR			(0x0200)
#define GICD_ICPENDR			(0x0280)
#define GICD_ISACTIVER			(0x0300)
#define GICD_ICACTIVER			(0x0380)
#define GICD_IPRIORITYR			(0x0400)
#define GICD_IPRIORITYR_END		(0x07f8 - 1)
#define GICD_ITARGETSR			(0x0800)
#define GICD_ICFGR			(0x0c00)
#define GICD_ICFGR_END			(0x0cfc - 1)
#define GICD_IGRPMODR			(0x0d00)
#define GICD_NSACR			(0x0e00)
#define GICD_SGIR			(0x0f00)
#define GICD_CPENDSGIR			(0x0f10)
#define GICD_SPENDSGIR			(0x0f20)
#define GICD_IROUTER			(0x6100)
#define GICD_PIDR2			(0xffe8)

#define GICR_CTLR			(0x0000)
#define GICR_IIDR			(0x0004)
#define GICR_TYPER			(0x0008)
#define GICR_STATUSR			(0X0010)
#define GICR_WAKER			(0x0014)
#define GICR_SETLPIR			(0x0040)
#define GICR_CLRLPIR			(0x0048)
#define GICR_SEIR			(0x0068)
#define GICR_PROPBASER			(0x0070)
#define GICR_PENDBASER			(0x0078)
#define GICR_INVLPIR			(0x00a0)
#define GICR_INVALLR			(0x00b0)
#define GICR_SYNCR			(0x00c0)
#define GICR_MOVLPIR			(0x0100)
#define GICR_MOVALLR			(0x0110)

#define GICR_IGROUPR0			(0x0080)
#define GICR_ISENABLER			(0x0100)
#define GICR_ICENABLER			(0x0180)
#define GICR_ISPENDR0			(0x0200)
#define GICR_ICPENDR0			(0x0280)
#define GICR_ISACTIVER0			(0x0300)
#define GICR_ICACTIVER0			(0x0380)
#define GICR_IPRIORITYR0		(0x0400)
#define GICR_ICFGR0			(0x0c00)
#define GICR_ICFGR1			(0x0c04)
#define GICR_IGRPMODR0			(0x0d00)
#define GICR_NSACR			(0x0e00)
#define GICR_PIDR2			(0xffe8)

#define GICH_VMCR_VENG0			(1 << 0)
#define GICH_VMCR_VENG1			(1 << 1)
#define GICH_VMCR_VACKCTL		(1 << 2)
#define GICH_VMCR_VFIQEN		(1 << 3)
#define GICH_VMCR_VCBPR			(1 << 4)
#define GICH_VMCR_VEOIM			(1 << 9)

#define GICH_HCR_EN       		(1 << 0)
#define GICH_HCR_UIE      		(1 << 1)
#define GICH_HCR_LRENPIE  		(1 << 2)
#define GICH_HCR_NPIE     		(1 << 3)
#define GICH_HCR_VGRP0EIE 		(1 << 4)
#define GICH_HCR_VGRP0DIE 		(1 << 5)
#define GICH_HCR_VGRP1EIE 		(1 << 6)
#define GICH_HCR_VGRP1DIE 		(1 << 7)

#define ICH_SGI_IRQMODE_SHIFT        	(40)
#define ICH_SGI_IRQMODE_MASK         	(0x1)
#define ICH_SGI_TARGET_OTHERS        	(1UL)
#define ICH_SGI_TARGET_LIST          	(0)
#define ICH_SGI_IRQ_SHIFT            	(24)
#define ICH_SGI_IRQ_MASK             	(0xf)
#define ICH_SGI_TARGETLIST_MASK      	(0xffff)
#define ICH_SGI_AFFx_MASK            	(0xff)
#define ICH_SGI_AFFINITY_LEVEL(x)    	(16 * (x))

#define GICV3_NR_LOCAL_IRQS	(32)
#define GICV3_NR_SGI		(16)

struct gic_context {
	uint64_t ich_lr0_el2;
	uint64_t ich_lr1_el2;
	uint64_t ich_lr2_el2;
	uint64_t ich_lr3_el2;
	uint64_t ich_lr4_el2;
	uint64_t ich_lr5_el2;
	uint64_t ich_lr6_el2;
	uint64_t ich_lr7_el2;
	uint64_t ich_lr8_el2;
	uint64_t ich_lr9_el2;
	uint64_t ich_lr10_el2;
	uint64_t ich_lr11_el2;
	uint64_t ich_lr12_el2;
	uint64_t ich_lr13_el2;
	uint64_t ich_lr14_el2;
	uint64_t ich_lr15_el2;
	uint32_t ich_ap0r2_el2;
	uint32_t ich_ap1r2_el2;
	uint32_t ich_ap0r1_el2;
	uint32_t ich_ap1r1_el2;
	uint32_t ich_ap0r0_el2;
	uint32_t ich_ap1r0_el2;
	uint32_t icc_sre_el1;
	uint32_t ich_vmcr_el2;
	uint32_t ich_hcr_el2;
} __align(sizeof(unsigned long));

struct gic_lr {
	uint64_t v_intid : 32;
	uint64_t p_intid : 10;
	uint64_t res0 : 6;
	uint64_t priority : 8;
	uint64_t res1 : 4;
	uint64_t group : 1;
	uint64_t hw : 1;
	uint64_t state : 2;
};

static inline uint64_t logic_cpu_to_irq_affinity(uint32_t c)
{
	return 0ul | (c << 0);
}

#endif
