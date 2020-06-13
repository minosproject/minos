#ifndef _MINOS_AARCH64_MMU_H_
#define _MINOS_AARCH64_MMU_H_

/*
 * stage 1 VMSAv8-64 Table Descriptors
 */
#define S1_DES_FAULT			(0b00 << 0)
#define S1_DES_BLOCK			(0b01 << 0)	/* level 1/2 */
#define S1_DES_TABLE			(0b11 << 0)	/* level 0/1/2 */
#define S1_DES_PAGE			(0b11 << 0)	/* level 3 */

#define S1_TABLE_NS			(1UL << 63)
#define S1_TABLE_AP			(0UL << 61)
#define S1_TABLE_XN			(1UL << 60)

/*
 * stage 1 VMSAv8-64 Block Descriptors
 */
#define S1_CONTIGUOUS			(1UL << 52)
#define S1_XN				(1UL << 54)
#define S1_ATTR_IDX(n)			((n & 0xf) << 2)
#define S1_NS				(1 << 5)

#define S1_AP_RW			(0b00 << 6)	// for EL2 ap[2] is valid
#define S1_AP_RO			(0b10 << 6)

#define S1_SH_NON			(0b00 << 8)
#define S1_SH_OUTER			(0b10 << 8)
#define S1_SH_INNER			(0b11 << 8)

#define S1_AF				(1 << 10)

/*
 * stage 1 VMSAv8-64 Page Descriptors
 */
#define S1_PAGE_CONTIGUOUS		(1UL << 52)
#define S1_PAGE_PXN			(1UL << 53)
#define S1_PAGE_XN			(1UL << 54)

#define S1_PAGE_NS			(1 << 5)

#define S1_PAGE_AP_RW			(0b00 << 6)	// for EL2 ap[2] is valid
#define S1_PAGE_AP_RO			(0b10 << 6)

#define S1_PAGE_SH_NON			(0b00 << 8)
#define S1_PAGE_SH_OUTER		(0b10 << 8)
#define S1_PAGE_SH_INNER		(0b11 << 8)

#define S1_PAGE_AF			(1 << 10)
#define S1_PAGE_nG			(1 << 11)

/*
 * stage 1 VMSAv8-64 Block Descriptors
 */
#define S1_BLK_CONTIGUOUS		(1UL << 52)
#define S1_BLK_XN			(1UL << 54)
#define S1_BLK_NS			(1 << 5)

#define S1_BLK_AP_RW			(0b00 << 6)	// for EL2 ap[2] is valid
#define S1_BLK_AP_RO			(0b10 << 6)

#define S1_BLK_SH_NON			(0b00 << 8)
#define S1_BLK_SH_OUTER			(0b10 << 8)
#define S1_BLK_SH_INNER			(0b11 << 8)

#define S1_BLK_AF			(1 << 10)
#define S1_BLK_nG			(1 << 11)

/*
 * Stage 2 VMSAv8-64 Table Descriptors
 */
#define S2_DES_FAULT			(0b00 << 0)
#define S2_DES_BLOCK			(0b01 << 0)
#define S2_DES_TABLE			(0b11 << 0)
#define S2_DES_PAGE			(0b11 << 0)

/*
 * Stage 2 VMSAv8-64 Page / Block Descriptors
 */
#define S2_CONTIGUOUS			(1UL << 52)
#define S2_XN				(1UL << 54)
#define S2_AF				(1UL << 10)

#define S2_SH_NON			(0b00 << 8)
#define S2_SH_OUTER			(0b10 << 8)
#define S2_SH_INNER			(0b11 << 8)

#define S2_S2AP_NON			(0b00 << 6)
#define S2_S2AP_RO			(0b01 << 6)
#define S2_S2AP_WO			(0b10 << 6)
#define S2_S2AP_RW			(0b11 << 6)

#define S2_MEMATTR_DEV_nGnRnE		(0b0000 << 2)
#define S2_MEMATTR_DEV_nGnRE		(0b0001 << 2)
#define S2_MEMATTR_DEV_nGRE		(0b0010 << 2)
#define S2_MEMATTR_DEV_GRE		(0b0011 << 2)

#define S2_MEMATTR_NORMAL_WB		(0b1111 << 2)
#define S2_MEMATTR_NORMAL_NC		(0b0101 << 2)
#define S2_MEMATTR_NORMAL_WT		(0b1010 << 2)

#define MT_DEVICE_nGnRnE		0
#define MT_DEVICE_nGnRE			1
#define MT_DEVICE_GRE			2
#define MT_NORMAL_NC			3
#define MT_NORMAL			4

#endif
