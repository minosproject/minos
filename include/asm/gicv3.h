#ifndef _MVISOR_GICV3_H_
#define _MVISOR_GICV3_H_

#define GICD_CTLR_ENABLE_GRP0		(1 << 0)
#define GICD_CTLR_ENABLE_GRP1NS		(1 << 1)
#define GICD_CTLR_ENABLE_GRP1A		(1 << 1)
#define GICD_CTLR_ENABLE_GRP1S		(1 << 2)
#define GICD_CTLR_ENABLE_ALL		((1 << 0) | (1 << 1) | (1 << 2))
#define GICD_CTLR_ARE_S			(1 << 4)
#define GICD_ARE_NS			(1 << 5)
#define GICD_DS				(1 << 6)
#define GICD_E1NWF			(1 << 7)

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
#define GICD_ICENABLER			(0x0180)
#define GICD_ISPENDR			(0x0200)
#define GICD_ICPENDR			(0x0280)
#define GICD_ISACTIVER			(0x0300)
#define GICD_ICACTIVER			(0x0380)
#define GICD_IPRIORITYR			(0x0400)
#define GICD_ITARGETSR			(0x0800)
#define GICD_ICFGR			(0x0c00)
#define GICD_IGRPMODR			(0x0d00)
#define GICD_NSACR			(0x0e00)
#define GICD_SGIR			(0x0f00)
#define GICD_CPENDSGIR			(0x0f10)
#define GICD_SPENDSGIR			(0x0f20)
#define GICD_IROUTER			(0x6000)

#define GICD_ISENABLER_OFFSET(id) \
	(GICD_ISENABLER + (((id >> 5) & 0x1f)) * sizeof(uint32_t))

#define GICD_ICENABLER_OFFSET(id) \
	(GICD_ICENABLER + (((id >> 5) & 0x1f)) * sizeof(uint32_t))

#define GICD_IPRIORITYR_OFFSET(id) \
	(GICD_IPRIORITYR + (id & 1023) * sizeof(uint8_t))

#define GICD_IROUTER_OFFSET(id) \
	(GICD_IROUTER + (id & 1023) * sizeof(uint64_t))

#define GICD_ITARGETSR_OFFSET(id) \
	(GICD_ITARGETSR + (id & 1023) * sizeof(uint8_t))

#define GICD_ICFGR_OFFSET(id) \
	(GICD_ICFGR + (id & 63) * sizeof(uint32_t))

#define GICD_ISPENDR_OFFSET(id) \
	(GICD_ISPENDR + (id & 31) * sizeof(uint32_t))

#define GICD_ICPENDR_OFFSET(id) \
	(GICR_ICPENDR + (id & 31) * sizeof(uint32_t))

#define GICD_IGROUPR_OFFSET(id) \
	(GICD_IGROUPR + (id & 31) * sizeof(uint32_t))

#define GICD_IGRPMODR_OFFSET(id) \
	(GICD_IGRPMODR + (id & 31) * sizeof(uint32_t))

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
#define GICR_ISPENDR			(0x0200)
#define GICR_ICPENDR			(0x0280)
#define GICR_ISACTIVER			(0x0300)
#define GICR_ICACTIVER			(0x0380)
#define GICR_IPRIORITYR			(0x0400)
#define GICR_ICNOFGR			(0x0c00)
#define GICR_IGRPMODR0			(0x0d00)
#define GICR_NSACR			(0x0e00)

#define GICR_IPRIORITYR_OFFSET(id) \
	(GICR_IPRIORITYR + ((id & 0x1f) * 8))

static inline uint64_t gicv3_pack_affinity(uint32_t aff3,
		uint32_t aff2, uint32_t aff1, uint32_t aff0)
{
	return ((((uint64_t)aff3 & 0xff) << 32) |
		((aff2 & 0xff) << 16) |
		((aff1 & 0xff) << 8) | aff0);
}

#endif
