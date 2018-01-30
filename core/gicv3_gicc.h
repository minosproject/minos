#ifndef _MVISOR_GICV3_GICC_H_
#define _MVISOR_GICV3_GICC_H_

#include <asm/gicv3.h>
#include <core/types.h>
#include <asm/GICv3_aliases.h>

#define ICC_SRE_SRE		(1 << 0)
#define ICC_SRE_DFB		(1 << 1)
#define ICC_SRE_DIB		(1 << 2)
#define ICC_SRE_ENABLE		(1 << 3)

#define ICC_IGRP_ENABLE		(1 << 0)
#define ICC_IGRP_ENABLE_GRP_1NS	(1 << 0)
#define ICC_IGRP_ENABLE_GRP_1S	(1 << 2)

#define ICC_CTLR_CBPR		(1 << 0)
#define ICC_CTLR_CBPR_EL1S	(1 << 0)
#define ICC_CTLR_EOI_MODE	(1 << 1)
#define ICC_CTLR_CBPR_EL1NS	(1 << 1)
#define ICC_CTLR_EOI_EL3	(1 << 2)
#define ICC_CTLR_EOI_EL1S	(1 << 3)
#define ICC_CTLR_EOI_EL1NS	(1 << 4)
#define ICC_CTLR_RM		(1 << 5)
#define ICC_CTLR_PMHE		(1 << 6)

#define ICC_SGIR_IRM_TARGET	(0)
#define ICC_SGIR_IRM_ALL	(1 << 40)

static inline void set_icc_sre_el2(uint64_t mode)
{
    asm("msr  "stringify(ICC_SRE_EL2)", %0\n; isb" :: "r" ((uint64_t)mode));
}

static inline uint64_t get_icc_sre_el2(void)
{
    uint64_t retc;

    asm("mrs  %0, "stringify(ICC_SRE_EL2)"\n" : "=r" (retc));

    return retc;
}

static inline void set_icc_sgi0r(uint8_t aff3, uint8_t aff2,
				uint8_t aff1, uint64_t irm,
				uint16_t targetlist, uint8_t intid)
{
    uint64_t packedbits = (((uint64_t)aff3 << 48) | ((uint64_t)aff2 << 32) | \
			   ((uint64_t)aff1 << 16) | irm | targetlist | \
			   ((uint64_t)(intid & 0x0f) << 24));


    asm("msr  "stringify(ICC_SGI0R_EL1)", %0\n; isb" :: "r" (packedbits));
}

static inline void set_icc_sgi1r(uint8_t aff3, uint8_t aff2,
				uint8_t aff1, uint64_t irm,
				uint16_t targetlist, uint8_t intid)
{
    uint64_t packedbits = (((uint64_t)aff3 << 48) | ((uint64_t)aff2 << 32) | \
			   ((uint64_t)aff1 << 16) | irm | targetlist |	\
			   ((uint64_t)(intid & 0x0f) << 24));

    asm("msr  "stringify(ICC_SGI1R_EL1)", %0\n; isb" :: "r" (packedbits));
}

#endif
