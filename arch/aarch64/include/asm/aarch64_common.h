#ifndef _MINOS_AARCH64_COMMON_H_
#define _MINOS_AARCH64_COMMON_H_

#include <minos/const.h>

#define ESR_ELx_EC_UNKNOWN	(0x00)
#define ESR_ELx_EC_WFx		(0x01)
/* Unallocated EC: 0x02 */
#define ESR_ELx_EC_CP15_32	(0x03)
#define ESR_ELx_EC_CP15_64	(0x04)
#define ESR_ELx_EC_CP14_MR	(0x05)
#define ESR_ELx_EC_CP14_LS	(0x06)
#define ESR_ELx_EC_FP_ASIMD	(0x07)
#define ESR_ELx_EC_CP10_ID	(0x08)
/* Unallocated EC: 0x09 - 0x0B */
#define ESR_ELx_EC_CP14_64	(0x0C)
/* Unallocated EC: 0x0d */
#define ESR_ELx_EC_ILL		(0x0E)
/* Unallocated EC: 0x0F - 0x10 */
#define ESR_ELx_EC_SVC32	(0x11)
#define ESR_ELx_EC_HVC32	(0x12)
#define ESR_ELx_EC_SMC32	(0x13)
/* Unallocated EC: 0x14 */
#define ESR_ELx_EC_SVC64	(0x15)
#define ESR_ELx_EC_HVC64	(0x16)
#define ESR_ELx_EC_SMC64	(0x17)
#define ESR_ELx_EC_SYS64	(0x18)
#define ESR_ELx_EC_SVE		(0x19)
/* Unallocated EC: 0x1A - 0x1E */
#define ESR_ELx_EC_IMP_DEF	(0x1f)
#define ESR_ELx_EC_IABT_LOW	(0x20)
#define ESR_ELx_EC_IABT_CUR	(0x21)
#define ESR_ELx_EC_PC_ALIGN	(0x22)
/* Unallocated EC: 0x23 */
#define ESR_ELx_EC_DABT_LOW	(0x24)
#define ESR_ELx_EC_DABT_CUR	(0x25)
#define ESR_ELx_EC_SP_ALIGN	(0x26)
/* Unallocated EC: 0x27 */
#define ESR_ELx_EC_FP_EXC32	(0x28)
/* Unallocated EC: 0x29 - 0x2B */
#define ESR_ELx_EC_FP_EXC64	(0x2C)
/* Unallocated EC: 0x2D - 0x2E */
#define ESR_ELx_EC_SERROR	(0x2F)
#define ESR_ELx_EC_BREAKPT_LOW	(0x30)
#define ESR_ELx_EC_BREAKPT_CUR	(0x31)
#define ESR_ELx_EC_SOFTSTP_LOW	(0x32)
#define ESR_ELx_EC_SOFTSTP_CUR	(0x33)
#define ESR_ELx_EC_WATCHPT_LOW	(0x34)
#define ESR_ELx_EC_WATCHPT_CUR	(0x35)
/* Unallocated EC: 0x36 - 0x37 */
#define ESR_ELx_EC_BKPT32	(0x38)
/* Unallocated EC: 0x39 */
#define ESR_ELx_EC_VECTOR32	(0x3A)
/* Unallocted EC: 0x3B */
#define ESR_ELx_EC_BRK64	(0x3C)
/* Unallocated EC: 0x3D - 0x3F */
#define ESR_ELx_EC_MAX		(0x3F)

#define ESR_ELx_EC_SHIFT	(26)
#define ESR_ELx_EC_MASK		(ULONG(0x3F) << ESR_ELx_EC_SHIFT)
#define ESR_ELx_EC(esr)		(((esr) & ESR_ELx_EC_MASK) >> ESR_ELx_EC_SHIFT)

#define ESR_ELx_IL_SHIFT	(25)
#define ESR_ELx_IL		(ULONG(1) << ESR_ELx_IL_SHIFT)
#define ESR_ELx_ISS_MASK	(ESR_ELx_IL - 1)

/* ISS field definitions shared by different classes */
#define ESR_ELx_WNR_SHIFT	(6)
#define ESR_ELx_WNR		(ULONG(1) << ESR_ELx_WNR_SHIFT)

/* Asynchronous Error Type */
#define ESR_ELx_IDS_SHIFT	(24)
#define ESR_ELx_IDS		(ULONG(1) << ESR_ELx_IDS_SHIFT)
#define ESR_ELx_AET_SHIFT	(10)
#define ESR_ELx_AET		(ULONG(0x7) << ESR_ELx_AET_SHIFT)

#define ESR_ELx_AET_UC		(ULONG(0) << ESR_ELx_AET_SHIFT)
#define ESR_ELx_AET_UEU		(ULONG(1) << ESR_ELx_AET_SHIFT)
#define ESR_ELx_AET_UEO		(ULONG(2) << ESR_ELx_AET_SHIFT)
#define ESR_ELx_AET_UER		(ULONG(3) << ESR_ELx_AET_SHIFT)
#define ESR_ELx_AET_CE		(ULONG(6) << ESR_ELx_AET_SHIFT)

/* Shared ISS field definitions for Data/Instruction aborts */
#define ESR_ELx_SET_SHIFT	(11)
#define ESR_ELx_SET_MASK	(ULONG(3) << ESR_ELx_SET_SHIFT)
#define ESR_ELx_FnV_SHIFT	(10)
#define ESR_ELx_FnV		(ULONG(1) << ESR_ELx_FnV_SHIFT)
#define ESR_ELx_EA_SHIFT	(9)
#define ESR_ELx_EA		(ULONG(1) << ESR_ELx_EA_SHIFT)
#define ESR_ELx_S1PTW_SHIFT	(7)
#define ESR_ELx_S1PTW		(ULONG(1) << ESR_ELx_S1PTW_SHIFT)

/* Shared ISS fault status code(IFSC/DFSC) for Data/Instruction aborts */
#define ESR_ELx_FSC		(0x3F)
#define ESR_ELx_FSC_TYPE	(0x3C)
#define ESR_ELx_FSC_EXTABT	(0x10)
#define ESR_ELx_FSC_SERROR	(0x11)
#define ESR_ELx_FSC_ACCESS	(0x08)
#define ESR_ELx_FSC_FAULT	(0x04)
#define ESR_ELx_FSC_PERM	(0x0C)

/* ISS field definitions for Data Aborts */
#define ESR_ELx_ISV_SHIFT	(24)
#define ESR_ELx_ISV		(ULONG(1) << ESR_ELx_ISV_SHIFT)
#define ESR_ELx_SAS_SHIFT	(22)
#define ESR_ELx_SAS		(ULONG(3) << ESR_ELx_SAS_SHIFT)
#define ESR_ELx_SSE_SHIFT	(21)
#define ESR_ELx_SSE		(ULONG(1) << ESR_ELx_SSE_SHIFT)
#define ESR_ELx_SRT_SHIFT	(16)
#define ESR_ELx_SRT_MASK	(ULONG(0x1F) << ESR_ELx_SRT_SHIFT)
#define ESR_ELx_SRT(val)	(((val) & ESR_ELx_SRT_MASK) >> ESR_ELx_SRT_SHIFT)
#define ESR_ELx_SF_SHIFT	(15)
#define ESR_ELx_SF 		(ULONG(1) << ESR_ELx_SF_SHIFT)
#define ESR_ELx_AR_SHIFT	(14)
#define ESR_ELx_AR 		(ULONG(1) << ESR_ELx_AR_SHIFT)
#define ESR_ELx_CM_SHIFT	(8)
#define ESR_ELx_CM 		(ULONG(1) << ESR_ELx_CM_SHIFT)

/* ISS field definitions for exceptions taken in to Hyp */
#define ESR_ELx_CV		(ULONG(1) << 24)
#define ESR_ELx_COND_SHIFT	(20)
#define ESR_ELx_COND_MASK	(ULONG(0xF) << ESR_ELx_COND_SHIFT)
#define ESR_ELx_WFx_ISS_WFE	(ULONG(1) << 0)
#define ESR_ELx_xVC_IMM_MASK	((1UL << 16) - 1)

#define FSC_FAULT       ESR_ELx_FSC_FAULT
#define FSC_ACCESS      ESR_ELx_FSC_ACCESS
#define FSC_PERM        ESR_ELx_FSC_PERM
#define FSC_SEA         ESR_ELx_FSC_EXTABT
#define FSC_SEA_TTW0    (0x14)
#define FSC_SEA_TTW1    (0x15)
#define FSC_SEA_TTW2    (0x16)
#define FSC_SEA_TTW3    (0x17)
#define FSC_SECC        (0x18)
#define FSC_SECC_TTW0   (0x1c)
#define FSC_SECC_TTW1   (0x1d)
#define FSC_SECC_TTW2   (0x1e)
#define FSC_SECC_TTW3   (0x1f)

#define DISR_EL1_IDS		(ULONG(1) << 24)
/*
 * DISR_EL1 and ESR_ELx share the bottom 13 bits, but the RES0 bits may mean
 * different things in the future...
 */
#define DISR_EL1_ESR_MASK	(ESR_ELx_AET | ESR_ELx_EA | ESR_ELx_FSC)

#define HPFAR_MASK	GENMASK(39, 4)

#define EC_TYPE_AARCH64		(0x1)
#define EC_TYPE_AARCH32		(0X2)
#define EC_TYPE_BOTH		(0x3)

/*
 * Current EL with SP0		0 - 3
 * Current EL with SPx		4 - 7
 * Lower EL with aarch64	8 - 11
 * Lower EL with aarch32	12 - 15
 */
#define VECTOR_C0_SYNC		0
#define VECTOR_C0_IRQ		1
#define VECTOR_C0_FIQ		2
#define VECTOR_C0_SERR		3
#define VECTOR_CX_SYNC		4
#define VECTOR_CX_IRQ		5
#define VECTOR_CX_FIQ		6
#define VECTOR_CX_SERR		7
#define VECTOR_L64_SYNC		8
#define VECTOR_L64_IRQ		9
#define VECTOR_L64_FIQ		10
#define VECTOR_L64_SERR		11
#define VECTOR_L32_SYNC		12
#define VECTOR_L32_IRQ		13
#define VECTOR_L32_FIQ		14
#define VECTOR_L32_SERR		15
#define VECTOR_MAX		16

#define AARCH64_SPSR_EL3h	0b1101		// el3 using sp_el3
#define AARCH64_SPSR_EL3t	0b1100		// el3 using sp_el0
#define AARCH64_SPSR_EL2h	0b1001		// el2 using sp_el2
#define AARCH64_SPSR_EL2t	0b1000		// el2 using sp_el0
#define AARCH64_SPSR_EL1h	0b0101		// el1 using sp_el1
#define AARCH64_SPSR_EL1t	0b0100		// el1 using sp_el0
#define AARCH64_SPSR_EL0t	0b0000		// el0 using sp_el0
#define AARCH64_SPSR_RW		(1 << 4)
#define AARCH64_SPSR_F		(1 << 6)
#define AARCH64_SPSR_I		(1 << 7)
#define AARCH64_SPSR_A		(1 << 8)
#define AARCH64_SPSR_D		(1 << 9)
#define AARCH64_SPSR_IL		(1 << 20)
#define AARCH64_SPSR_SS		(1 << 21)
#define AARCH64_SPSR_V		(1 << 28)
#define AARCH64_SPSR_C		(1 << 29)
#define AARCH64_SPSR_Z		(1 << 30)
#define AARCH64_SPSR_N		(1 << 31)

#define AARCH32_USER		0b0000
#define AARCH32_FIQ		0b0001
#define AARCH32_IRQ		0b0010
#define AARCH32_SVC		0b0011
#define AARCH32_MON		0b0110
#define AARCH32_ABT		0b0111
#define AARCH32_HYP		0b1010
#define AARCH32_UND		0b1011
#define AARCH32_SYSTEM		0b1111

#define MODE_EL3		(0x3UL)
#define MODE_EL2		(0x2UL)
#define MODE_EL1		(0x1UL)
#define MODE_EL0		(0x0UL)
#define MODE_EL_SHIFT		(0x2UL)
#define MODE_EL_MASK		(0x3UL)
#define GET_EL(mode)		(((mode) >> MODE_EL_SHIFT) & MODE_EL_MASK)

#define MPIDR_EL1_AFF3_LSB	32
#define MPIDR_EL1_U		(1 << 30)
#define MPIDR_EL1_MT		(1 << 24)
#define MPIDR_EL1_AFF2_LSB	16
#define MPIDR_EL1_AFF1_LSB	8
#define MPIDR_EL1_AFF0_LSB	0
#define MPIDR_EL1_AFF_WIDTH	8
#define MIPIDR_AFF_SHIFT	2

#define DCZID_EL0_BS_LSB	0
#define DCZID_EL0_BS_WIDTH	4
#define DCZID_EL0_DZP_LSB	5
#define DCZID_EL0_DZP		(1 << 5)

#define SCTLR_EL1_UCI		(1 << 26)
#define SCTLR_ELx_EE		(1 << 25)
#define SCTLR_EL1_E0E		(1 << 24)
#define SCTLR_ELx_WXN		(1 << 19)
#define SCTLR_EL1_nTWE		(1 << 18)
#define SCTLR_EL1_nTWI		(1 << 16)
#define SCTLR_EL1_UCT		(1 << 15)
#define SCTLR_EL1_DZE		(1 << 14)
#define SCTLR_ELx_I		(1 << 12)
#define SCTLR_EL1_UMA		(1 << 9)
#define SCTLR_EL1_SED		(1 << 8)
#define SCTLR_EL1_ITD		(1 << 7)
#define SCTLR_EL1_THEE		(1 << 6)
#define SCTLR_EL1_CP15BEN	(1 << 5)
#define SCTLR_EL1_SA0		(1 << 4)
#define SCTLR_ELx_SA		(1 << 3)
#define SCTLR_ELx_C		(1 << 2)
#define SCTLR_ELx_A		(1 << 1)
#define SCTLR_ELx_M		(1 << 0)

#define SCTLR_ELx_C_BIT		(2)
#define SCTLR_ELx_A_BIT		(1)
#define SCTLR_ELx_M_BIT		(0)

#define CPACR_EL1_TTA		(1 << 28)
#define CPACR_EL1_FPEN		(3 << 20)

#define CPTR_ELx_TCPAC		(1 << 31)
#define CPTR_ELx_TTA		(1 << 20)
#define CPTR_ELx_TFP		(1 << 10)

#define SCR_EL3_TWE		(1 << 13)
#define SCR_EL3_TWI		(1 << 12)
#define SCR_EL3_ST		(1 << 11)
#define SCR_EL3_RW		(1 << 10)
#define SCR_EL3_SIF		(1 << 9)
#define SCR_EL3_HCE		(1 << 8)
#define SCR_EL3_SMD		(1 << 7)
#define SCR_EL3_EA		(1 << 3)
#define SCR_EL3_FIQ		(1 << 2)
#define SCR_EL3_IRQ		(1 << 1)
#define SCR_EL3_NS		(1 << 0)

#define HCR_EL2_VM		(1ul << 0)
#define HCR_EL2_SWIO		(1ul << 1)
#define HCR_EL2_PTW		(1ul << 2)
#define HCR_EL2_FMO		(1ul << 3)
#define HCR_EL2_IMO		(1ul << 4)
#define HCR_EL2_AMO		(1ul << 5)
#define HCR_EL2_VF		(1ul << 6)
#define HCR_EL2_VI		(1ul << 7)
#define HCR_EL2_VSE		(1ul << 8)
#define HCR_EL2_FB		(1ul << 9)
#define HCR_EL2_BSU_IS		(1ul << 10)
#define HCR_EL2_BSU_OS		(2ul << 10)
#define HCR_EL2_BSU_FS		(3ul << 10)
#define HCR_EL2_DC		(1ul << 12)
#define HCR_EL2_TWI		(1ul << 13)
#define HCR_EL2_TWE		(1ul << 14)
#define HCR_EL2_TID0		(1ul << 15)
#define HCR_EL2_TID1		(1ul << 16)
#define HCR_EL2_TID2		(1ul << 17)
#define HCR_EL2_TID3		(1ul << 18)
#define HCR_EL2_TSC		(1ul << 19)
#define HCR_EL2_TIDCP		(1ul << 20)
#define HCR_EL2_TACR		(1ul << 21)
#define HCR_EL2_TSW		(1ul << 22)
#define HCR_EL2_TPC		(1ul << 23)
#define HCR_EL2_TPU		(1ul << 24)
#define HCR_EL2_TTLB		(1ul << 25)
#define HCR_EL2_TVM		(1ul << 26)
#define HCR_EL2_TGE		(1ul << 27)
#define HCR_EL2_TDZ		(1ul << 28)
#define HCR_EL2_HCD		(1ul << 29)
#define HCR_EL2_TRVM		(1ul << 30)
#define HCR_EL2_RW		(1ul << 31)
#define HCR_EL2_CD		(1ul << 32)
#define HCR_EL2_ID		(1ul << 33)
#define HCR_EL2_E2H		(1ul << 34)

#define LOUIS_SHIFT		(21)
#define LOC_SHIFT		(24)
#define CLIDR_FIELD_WIDTH	(3)

#define DAIF_F_BIT		6
#define DAIF_I_BIT		7
#define DAIF_A_BIT		8
#define DAIF_D_BIT		9

#define LEVEL_SHIFT		(1)

/*
 * TCR flags.
 */
#define TCR_T0SZ_OFFSET		0
#define TCR_T1SZ_OFFSET		16
#define TCR_T0SZ(x)		((UL(64) - (x)) << TCR_T0SZ_OFFSET)
#define TCR_T1SZ(x)		((UL(64) - (x)) << TCR_T1SZ_OFFSET)
#define TCR_TxSZ(x)		(TCR_T0SZ(x) | TCR_T1SZ(x))
#define TCR_TxSZ_WIDTH		6
#define TCR_T0SZ_MASK		(((UL(1) << TCR_TxSZ_WIDTH) - 1) << TCR_T0SZ_OFFSET)

#define TCR_IRGN0_SHIFT		8
#define TCR_IRGN0_MASK		(UL(3) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_NC		(UL(0) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_WBWA		(UL(1) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_WT		(UL(2) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_WBnWA		(UL(3) << TCR_IRGN0_SHIFT)

#define TCR_IRGN1_SHIFT		24
#define TCR_IRGN1_MASK		(UL(3) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_NC		(UL(0) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_WBWA		(UL(1) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_WT		(UL(2) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_WBnWA		(UL(3) << TCR_IRGN1_SHIFT)

#define TCR_IRGN_NC		(TCR_IRGN0_NC | TCR_IRGN1_NC)
#define TCR_IRGN_WBWA		(TCR_IRGN0_WBWA | TCR_IRGN1_WBWA)
#define TCR_IRGN_WT		(TCR_IRGN0_WT | TCR_IRGN1_WT)
#define TCR_IRGN_WBnWA		(TCR_IRGN0_WBnWA | TCR_IRGN1_WBnWA)
#define TCR_IRGN_MASK		(TCR_IRGN0_MASK | TCR_IRGN1_MASK)

#define TCR_ORGN0_SHIFT		10
#define TCR_ORGN0_MASK		(UL(3) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_NC		(UL(0) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_WBWA		(UL(1) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_WT		(UL(2) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_WBnWA		(UL(3) << TCR_ORGN0_SHIFT)

#define TCR_ORGN1_SHIFT		26
#define TCR_ORGN1_MASK		(UL(3) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_NC		(UL(0) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_WBWA		(UL(1) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_WT		(UL(2) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_WBnWA		(UL(3) << TCR_ORGN1_SHIFT)

#define TCR_ORGN_NC		(TCR_ORGN0_NC | TCR_ORGN1_NC)
#define TCR_ORGN_WBWA		(TCR_ORGN0_WBWA | TCR_ORGN1_WBWA)
#define TCR_ORGN_WT		(TCR_ORGN0_WT | TCR_ORGN1_WT)
#define TCR_ORGN_WBnWA		(TCR_ORGN0_WBnWA | TCR_ORGN1_WBnWA)
#define TCR_ORGN_MASK		(TCR_ORGN0_MASK | TCR_ORGN1_MASK)

#define TCR_SH0_SHIFT		12
#define TCR_SH0_MASK		(UL(3) << TCR_SH0_SHIFT)
#define TCR_SH0_INNER		(UL(3) << TCR_SH0_SHIFT)

#define TCR_SH1_SHIFT		28
#define TCR_SH1_MASK		(UL(3) << TCR_SH1_SHIFT)
#define TCR_SH1_INNER		(UL(3) << TCR_SH1_SHIFT)
#define TCR_SHARED		(TCR_SH0_INNER | TCR_SH1_INNER)

#define TCR_TG0_SHIFT		14
#define TCR_TG0_MASK		(UL(3) << TCR_TG0_SHIFT)
#define TCR_TG0_4K		(UL(0) << TCR_TG0_SHIFT)
#define TCR_TG0_64K		(UL(1) << TCR_TG0_SHIFT)
#define TCR_TG0_16K		(UL(2) << TCR_TG0_SHIFT)

#define TCR_TG1_SHIFT		30
#define TCR_TG1_MASK		(UL(3) << TCR_TG1_SHIFT)
#define TCR_TG1_16K		(UL(1) << TCR_TG1_SHIFT)
#define TCR_TG1_4K		(UL(2) << TCR_TG1_SHIFT)
#define TCR_TG1_64K		(UL(3) << TCR_TG1_SHIFT)

#define TCR_IPS_SHIFT		32
#define TCR_IPS_MASK		(UL(7) << TCR_IPS_SHIFT)
#define TCR_A1			(UL(1) << 22)
#define TCR_ASID16		(UL(1) << 36)
#define TCR_TBI0		(UL(1) << 37)
#define TCR_HA			(UL(1) << 39)
#define TCR_HD			(UL(1) << 40)
#define TCR_NFD1		(UL(1) << 54)

#endif
