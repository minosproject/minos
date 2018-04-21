#ifndef _MVISOR_AARCH64_COMMON_H_
#define _MVISOR_AARCH64_COMMON_H_

#define AARCH64_SPSR_EL3h	0b1101
#define AARCH64_SPSR_EL3t	0b1100
#define AARCH64_SPSR_EL2h	0b1001
#define AARCH64_SPSR_EL2t	0b1000
#define AARCH64_SPSR_EL1h	0b0101
#define AARCH64_SPSR_EL1t	0b0100
#define AARCH64_SPSR_EL0t	0b0000
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

#define MPIDR_EL1_AFF3_LSB	32
#define MPIDR_EL1_U		(1 << 30)
#define MPIDR_EL1_MT		(1 << 24)
#define MPIDR_EL1_AFF2_LSB	16
#define MPIDR_EL1_AFF1_LSB	8
#define MPIDR_EL1_AFF0_LSB	0
#define MPIDR_EL1_AFF_WIDTH	8

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

#define HCR_EL2_VM		(1 << 0)
#define HCR_EL2_SWIO		(1 << 1)
#define HCR_EL2_PTW		(1 << 2)
#define HCR_EL2_FMO		(1 << 3)
#define HCR_EL2_IMO		(1 << 4)
#define HCR_EL2_AMO		(1 << 5)
#define HCR_EL2_VF		(1 << 6)
#define HCR_EL2_VI		(1 << 7)
#define HCR_EL2_VSE		(1 << 8)
#define HCR_EL2_FB		(1 << 9)
#define HCR_EL2_BSU_IS		(1 << 10)
#define HCR_EL2_BSU_OS		(2 << 10)
#define HCR_EL2_BSU_FS		(3 << 10)
#define HCR_EL2_DC		(1 << 12)
#define HCR_EL2_TWI		(1 << 13)
#define HCR_EL2_TWE		(1 << 14)
#define HCR_EL2_TID0		(1 << 15)
#define HCR_EL2_TID1		(1 << 16)
#define HCR_EL2_TID2		(1 << 17)
#define HCR_EL2_TID3		(1 << 18)
#define HCR_EL2_TSC		(1 << 19)
#define HCR_EL2_TIDCP		(1 << 20)
#define HCR_EL2_TACR		(1 << 21)
#define HCR_EL2_TSW		(1 << 22)
#define HCR_EL2_TPC		(1 << 23)
#define HCR_EL2_TPU		(1 << 24)
#define HCR_EL2_TTLB		(1 << 25)
#define HCR_EL2_TVM		(1 << 26)
#define HCR_EL2_TGE		(1 << 27)
#define HCR_EL2_TDZ		(1 << 28)
#define HCR_EL2_HVC		(1 << 29)
#define HCR_EL2_TRVM		(1 << 30)
#define HCR_EL2_RW		(1 << 31)
#define HCR_EL2_CD		(1 << 32)
#define HCR_EL2_ID		(1 << 33)

#define LOUIS_SHIFT		(21)
#define LOC_SHIFT		(24)
#define CLIDR_FIELD_WIDTH	(3)

/* CSSELR definitions */
#define LEVEL_SHIFT		(1)

/* D$ set/way op type defines */
#define DCISW			(0x0)
#define DCCISW			(0x1)
#define DCCSW			(0x2)

#endif
