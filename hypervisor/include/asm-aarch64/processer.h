#ifndef _MINOS_PROCESSER_H_
#define _MINOS_PROCESSER_H_

#include <minos/types.h>

#define HPFAR_MASK	GENMASK(39, 4)

/*
 * copied from xen defination
 */

/* ESR.EC == ESR_CP{15,14,10}_32 */
#define ESR_CP32_OP2_MASK (0x000e0000)
#define ESR_CP32_OP2_SHIFT (17)
#define ESR_CP32_OP1_MASK (0x0001c000)
#define ESR_CP32_OP1_SHIFT (14)
#define ESR_CP32_CRN_MASK (0x00003c00)
#define ESR_CP32_CRN_SHIFT (10)
#define ESR_CP32_CRM_MASK (0x0000001e)
#define ESR_CP32_CRM_SHIFT (1)
#define ESR_CP32_REGS_MASK (ESR_CP32_OP1_MASK|ESR_CP32_OP2_MASK|\
                            ESR_CP32_CRN_MASK|ESR_CP32_CRM_MASK)

/* ESR.EC == ESR_CP{15,14}_64 */
#define ESR_CP64_OP1_MASK (0x000f0000)
#define ESR_CP64_OP1_SHIFT (16)
#define ESR_CP64_CRM_MASK (0x0000001e)
#define ESR_CP64_CRM_SHIFT (1)
#define ESR_CP64_REGS_MASK (ESR_CP64_OP1_MASK|ESR_CP64_CRM_MASK)

/* ESR.EC == ESR_SYSREG */
#define ESR_SYSREG_OP0_MASK (0x00300000)
#define ESR_SYSREG_OP0_SHIFT (20)
#define ESR_SYSREG_OP1_MASK (0x0001c000)
#define ESR_SYSREG_OP1_SHIFT (14)
#define ESR_SYSREG_CRN_MASK (0x00003c00)
#define ESR_SYSREG_CRN_SHIFT (10)
#define ESR_SYSREG_CRM_MASK (0x0000001e)
#define ESR_SYSREG_CRM_SHIFT (1)
#define ESR_SYSREG_OP2_MASK (0x000e0000)
#define ESR_SYSREG_OP2_SHIFT (17)
#define ESR_SYSREG_REGS_MASK (ESR_SYSREG_OP0_MASK|ESR_SYSREG_OP1_MASK|\
                              ESR_SYSREG_CRN_MASK|ESR_SYSREG_CRM_MASK|\
                              ESR_SYSREG_OP2_MASK)

/* ESR.EC == ESR_{HVC32, HVC64, SMC64, SVC32, SVC64} */
#define ESR_XXC_IMM_MASK     (0xffff)

#define __ESR_SYSREG_c0  0
#define __ESR_SYSREG_c1  1
#define __ESR_SYSREG_c2  2
#define __ESR_SYSREG_c3  3
#define __ESR_SYSREG_c4  4
#define __ESR_SYSREG_c5  5
#define __ESR_SYSREG_c6  6
#define __ESR_SYSREG_c7  7
#define __ESR_SYSREG_c8  8
#define __ESR_SYSREG_c9  9
#define __ESR_SYSREG_c10 10
#define __ESR_SYSREG_c11 11
#define __ESR_SYSREG_c12 12
#define __ESR_SYSREG_c13 13
#define __ESR_SYSREG_c14 14
#define __ESR_SYSREG_c15 15

#define __ESR_SYSREG_0   0
#define __ESR_SYSREG_1   1
#define __ESR_SYSREG_2   2
#define __ESR_SYSREG_3   3
#define __ESR_SYSREG_4   4
#define __ESR_SYSREG_5   5
#define __ESR_SYSREG_6   6
#define __ESR_SYSREG_7   7

/* These are used to decode traps with ESR.EC==ESR_EC_SYSREG */
#define ESR_SYSREG(op0,op1,crn,crm,op2) \
    (((__ESR_SYSREG_##op0) << ESR_SYSREG_OP0_SHIFT) | \
     ((__ESR_SYSREG_##op1) << ESR_SYSREG_OP1_SHIFT) | \
     ((__ESR_SYSREG_##crn) << ESR_SYSREG_CRN_SHIFT) | \
     ((__ESR_SYSREG_##crm) << ESR_SYSREG_CRM_SHIFT) | \
     ((__ESR_SYSREG_##op2) << ESR_SYSREG_OP2_SHIFT))

#define ESR_SYSREG_DCISW          ESR_SYSREG(1,0,c7,c6,2)
#define ESR_SYSREG_DCCSW          ESR_SYSREG(1,0,c7,c10,2)
#define ESR_SYSREG_DCCISW         ESR_SYSREG(1,0,c7,c14,2)
#define ESR_SYSREG_DCZVA	  ESR_SYSREG(1,3,c7,c4,1)

#define ESR_SYSREG_MDSCR_EL1      ESR_SYSREG(2,0,c0,c2,2)
#define ESR_SYSREG_MDRAR_EL1      ESR_SYSREG(2,0,c1,c0,0)
#define ESR_SYSREG_OSLAR_EL1      ESR_SYSREG(2,0,c1,c0,4)
#define ESR_SYSREG_OSLSR_EL1      ESR_SYSREG(2,0,c1,c1,4)
#define ESR_SYSREG_OSDLR_EL1      ESR_SYSREG(2,0,c1,c3,4)
#define ESR_SYSREG_DBGPRCR_EL1    ESR_SYSREG(2,0,c1,c4,4)
#define ESR_SYSREG_MDCCSR_EL0     ESR_SYSREG(2,3,c0,c1,0)

#define ESR_SYSREG_DBGBVRn_EL1(n) ESR_SYSREG(2,0,c0,c##n,4)
#define ESR_SYSREG_DBGBCRn_EL1(n) ESR_SYSREG(2,0,c0,c##n,5)
#define ESR_SYSREG_DBGWVRn_EL1(n) ESR_SYSREG(2,0,c0,c##n,6)
#define ESR_SYSREG_DBGWCRn_EL1(n) ESR_SYSREG(2,0,c0,c##n,7)

#define ESR_SYSREG_DBG_CASES(REG) case ESR_SYSREG_##REG##n_EL1(0):  \
                                  case ESR_SYSREG_##REG##n_EL1(1):  \
                                  case ESR_SYSREG_##REG##n_EL1(2):  \
                                  case ESR_SYSREG_##REG##n_EL1(3):  \
                                  case ESR_SYSREG_##REG##n_EL1(4):  \
                                  case ESR_SYSREG_##REG##n_EL1(5):  \
                                  case ESR_SYSREG_##REG##n_EL1(6):  \
                                  case ESR_SYSREG_##REG##n_EL1(7):  \
                                  case ESR_SYSREG_##REG##n_EL1(8):  \
                                  case ESR_SYSREG_##REG##n_EL1(9):  \
                                  case ESR_SYSREG_##REG##n_EL1(10): \
                                  case ESR_SYSREG_##REG##n_EL1(11): \
                                  case ESR_SYSREG_##REG##n_EL1(12): \
                                  case ESR_SYSREG_##REG##n_EL1(13): \
                                  case ESR_SYSREG_##REG##n_EL1(14): \
                                  case ESR_SYSREG_##REG##n_EL1(15)

#define ESR_SYSREG_SCTLR_EL1      ESR_SYSREG(3,0,c1, c0,0)
#define ESR_SYSREG_ACTLR_EL1      ESR_SYSREG(3,0,c1, c0,1)
#define ESR_SYSREG_TTBR0_EL1      ESR_SYSREG(3,0,c2, c0,0)
#define ESR_SYSREG_TTBR1_EL1      ESR_SYSREG(3,0,c2, c0,1)
#define ESR_SYSREG_TCR_EL1        ESR_SYSREG(3,0,c2, c0,2)
#define ESR_SYSREG_AFSR0_EL1      ESR_SYSREG(3,0,c5, c1,0)
#define ESR_SYSREG_AFSR1_EL1      ESR_SYSREG(3,0,c5, c1,1)
#define ESR_SYSREG_ESR_EL1        ESR_SYSREG(3,0,c5, c2,0)
#define ESR_SYSREG_FAR_EL1        ESR_SYSREG(3,0,c6, c0,0)
#define ESR_SYSREG_PMINTENSET_EL1 ESR_SYSREG(3,0,c9,c14,1)
#define ESR_SYSREG_PMINTENCLR_EL1 ESR_SYSREG(3,0,c9,c14,2)
#define ESR_SYSREG_MAIR_EL1       ESR_SYSREG(3,0,c10,c2,0)
#define ESR_SYSREG_AMAIR_EL1      ESR_SYSREG(3,0,c10,c3,0)
#define ESR_SYSREG_ICC_SGI1R_EL1  ESR_SYSREG(3,0,c12,c11,5)
#define ESR_SYSREG_ICC_ASGI1R_EL1 ESR_SYSREG(3,1,c12,c11,6)
#define ESR_SYSREG_ICC_SGI0R_EL1  ESR_SYSREG(3,2,c12,c11,7)
#define ESR_SYSREG_ICC_SRE_EL1    ESR_SYSREG(3,0,c12,c12,5)
#define ESR_SYSREG_CONTEXTIDR_EL1 ESR_SYSREG(3,0,c13,c0,1)

#define ESR_SYSREG_PMCR_EL0       ESR_SYSREG(3,3,c9,c12,0)
#define ESR_SYSREG_PMCNTENSET_EL0 ESR_SYSREG(3,3,c9,c12,1)
#define ESR_SYSREG_PMCNTENCLR_EL0 ESR_SYSREG(3,3,c9,c12,2)
#define ESR_SYSREG_PMOVSCLR_EL0   ESR_SYSREG(3,3,c9,c12,3)
#define ESR_SYSREG_PMSWINC_EL0    ESR_SYSREG(3,3,c9,c12,4)
#define ESR_SYSREG_PMSELR_EL0     ESR_SYSREG(3,3,c9,c12,5)
#define ESR_SYSREG_PMCEID0_EL0    ESR_SYSREG(3,3,c9,c12,6)
#define ESR_SYSREG_PMCEID1_EL0    ESR_SYSREG(3,3,c9,c12,7)

#define ESR_SYSREG_PMCCNTR_EL0    ESR_SYSREG(3,3,c9,c13,0)
#define ESR_SYSREG_PMXEVTYPER_EL0 ESR_SYSREG(3,3,c9,c13,1)
#define ESR_SYSREG_PMXEVCNTR_EL0  ESR_SYSREG(3,3,c9,c13,2)

#define ESR_SYSREG_PMUSERENR_EL0  ESR_SYSREG(3,3,c9,c14,0)
#define ESR_SYSREG_PMOVSSET_EL0   ESR_SYSREG(3,3,c9,c14,3)

#define ESR_SYSREG_CNTPCT_EL0     ESR_SYSREG(3,3,c14,c0,0)
#define ESR_SYSREG_CNTP_TVAL_EL0  ESR_SYSREG(3,3,c14,c2,0)
#define ESR_SYSREG_CNTP_CTL_EL0   ESR_SYSREG(3,3,c14,c2,1)
#define ESR_SYSREG_CNTP_CVAL_EL0  ESR_SYSREG(3,3,c14,c2,2)

#define FSC_TYPE_MASK (_AC(0x3,U)<<4)
#define FSC_TYPE_FAULT (_AC(0x00,U)<<4)
#define FSC_TYPE_ABT   (_AC(0x01,U)<<4)
#define FSC_TYPE_OTH   (_AC(0x02,U)<<4)
#define FSC_TYPE_IMPL  (_AC(0x03,U)<<4)

#define FSC_FLT_TRANS  (0x04)
#define FSC_FLT_ACCESS (0x08)
#define FSC_FLT_PERM   (0x0c)
#define FSC_SEA        (0x10) /* Synchronous External Abort */
#define FSC_SPE        (0x18) /* Memory Access Synchronous Parity Error */
#define FSC_APE        (0x11) /* Memory Access Asynchronous Parity Error */
#define FSC_SEATT      (0x14) /* Sync. Ext. Abort Translation Table */
#define FSC_SPETT      (0x1c) /* Sync. Parity. Error Translation Table */
#define FSC_AF         (0x21) /* Alignment Fault */
#define FSC_DE         (0x22) /* Debug Event */
#define FSC_LKD        (0x34) /* Lockdown Abort */
#define FSC_CPR        (0x3a) /* Coprocossor Abort */

#define FSC_LL_MASK    (_AC(0x03,U)<<0)

#endif
