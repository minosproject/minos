#ifndef __MINOS_ARM64_STAGE1_H__
#define __MINOS_ARM64_STAGE1_H__

#include <minos/const.h>

/*
 * stage 1 VMSAv8-64 Table Descriptors
 */
#define S1_DES_FAULT		(0b00 << 0)
#define S1_DES_BLOCK		(0b01 << 0)	/* level 1/2 */
#define S1_DES_TABLE		(0b11 << 0)	/* level 0/1/2 */
#define S1_DES_PAGE		(0b11 << 0)	/* level 3 */

#define S1_TABLE_NS		(UL(1) << 63)
#define S1_TABLE_AP		(0)
#define S1_TABLE_XN		(UL(1) << 60)

#define S1_TABLE_UAP		(UL(1) << 61)
#define S1_TABLE_UXN		(UL(1) << 60)

#define S1_CONTIGUOUS		(UL(1) << 52)
#define S1_PXN			(UL(1) << 53)
#define S1_XN			(UL(1) << 54)

#define S1_PFNMAP		(UL(1) << 55)	// 55 - 58 is for software
#define S1_DEVMAP		(UL(1) << 56)	// 55 - 58 is for software
#define S1_SHARED		(UL(1) << 57)	// 55 - 58 is for software

#define S1_NS			(1 << 5)

#define S1_AP_RW		(0b00 << 6)	// for EL2 ap[2] is valid
#define S1_AP_RW_URW		(0b01 << 6)
#define S1_AP_RO		(0b10 << 6)
#define S1_AP_RO_URO		(0b11 << 6)

#define S1_SH_NON		(0b00 << 8)
#define S1_SH_OUTER		(0b10 << 8)
#define S1_SH_INNER		(0b11 << 8)

#define S1_AF			(1 << 10)
#define S1_nG			(1 << 11)

#define S1_ATTR_IDX(n)		((n & 0xf) << 2)

/*
 * #define ioremap_nocache(addr, size)     __ioremap((addr), (size), __pgprot(PROT_DEVICE_nGnRE))
 * #define ioremap_wc(addr, size)          __ioremap((addr), (size), __pgprot(PROT_NORMAL_NC))
 * #define ioremap_wt(addr, size)          __ioremap((addr), (size), __pgprot(PROT_DEVICE_nGnRE))
 * #define ioremap(addr, size)             __ioremap((addr), (size), __pgprot(PROT_DEVICE_nGnRE))
 */
#define MT_DEVICE_nGnRnE	0	// Device-nGnRnE memory
#define MT_DEVICE_nGnRE		1	// Device-nGnRE memory
#define MT_DEVICE_GRE		2	// Device-GRE memory
#define MT_NORMAL_NC		3	// Normal memory, Inner Non-cacheable, Outer Non-cacheable
#define MT_NORMAL		4	// Normal memory, Outer Write-Back Non-transient, Inner Write-Back Non-transient
#define MT_NORMAL_WT		5	// Normal memory, Outer Write-Through Non-transient, Inner Write-Through Non-transient

#define S1_PAGE_NORMAL		(S1_DES_PAGE | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_NORMAL))
#define S1_PAGE_NORMAL_NC	(S1_DES_PAGE | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_NORMAL_NC))
#define S1_PAGE_DEVICE		(S1_DES_PAGE | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_DEVICE_nGnRE))
// #define S1_PAGE_DEVICE		(S1_DES_PAGE | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_DEVICE_nGnRnE))
#define S1_PAGE_WT		(S1_DES_PAGE | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_NORMAL_WT))

#define S1_BLOCK_NORMAL		(S1_DES_BLOCK | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_NORMAL))
#define S1_BLOCK_NORMAL_NC	(S1_DES_BLOCK | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_NORMAL_NC))
#define S1_BLOCK_DEVICE		(S1_DES_BLOCK | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_DEVICE_nGnRE))
// #define S1_BLOCK_DEVICE		(S1_DES_BLOCK | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_DEVICE_nGnRnE))
#define S1_BLOCK_WT		(S1_DES_BLOCK | S1_AF | S1_NS | S1_SH_INNER | S1_ATTR_IDX(MT_NORMAL_WT))

#define BOOTMEM_CODE_ATTR	(S1_ATTR_IDX(MT_NORMAL) | S1_DES_PAGE | S1_NS | S1_AP_RO | S1_SH_INNER | S1_AF)
#define BOOTMEM_DATA_ATTR	(S1_ATTR_IDX(MT_NORMAL) | S1_DES_PAGE | S1_NS | S1_AP_RW | S1_SH_INNER | S1_AF | S1_XN)
#define BOOTMEM_DATA_RO_ATTR	(S1_ATTR_IDX(MT_NORMAL) | S1_DES_PAGE | S1_NS | S1_AP_RO | S1_SH_INNER | S1_AF | S1_XN)
#define BOOTMEM_INIT_ATTR	(S1_ATTR_IDX(MT_NORMAL) | S1_DES_PAGE | S1_NS | S1_AP_RW | S1_SH_INNER | S1_AF)
#define BOOTMEM_IO_ATTR		(S1_ATTR_IDX(MT_DEVICE_nGnRnE) | S1_DES_PAGE | S1_NS | S1_AP_RW | S1_SH_INNER | S1_AF | S1_XN)
#define BOOTMEM_DATA_BLK_ATTR	(S1_ATTR_IDX(MT_NORMAL) | S1_DES_BLOCK | S1_NS | S1_AP_RW | S1_SH_INNER | S1_AF | S1_XN | S1_PXN)

#endif
