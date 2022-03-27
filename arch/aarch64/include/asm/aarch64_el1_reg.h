/*
 * Copyright (C) 2020 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MINOS_AARCH64_EL1_REG_H__
#define __MINOS_ARRCH64_EL1_REG_H__

#include <asm/aarch64_common.h>

#define ARM64_SCTLR		SCTLR_EL1
#define ARM64_CPACR		CAPACR_EL1
#define ARM64_TRFCR		TRFCR_EL1
#define ARM64_TTBR0		TTBR0_EL1
#define ARM64_TTBR1		TTBR1_EL1
#define ARM64_TCR		TCR_EL1
#define ARM64_AFSR0		AFSR0_EL1
#define ARM64_AFSR1		AFSR1_EL1
#define ARM64_ESR		ESR_EL1
#define ARM64_FAR		FAR_EL1
#define ARM64_MAIR		MAIR_EL1
#define ARM64_AMAIR		AMAIR_EL1
#define ARM64_VBAR		VBAR_EL1
#define ARM64_CONTEXTIDR	CONTEXTIDR_EL1
#define ARM64_CNTKCTL		CNTHCTL_EL1
#define ARM64_SPSR		SPSR_EL1
#define ARM64_ELR		ELR_EL1
#define ARM64_TPIDR		TPIDR_EL1
#define ARM64_CNTSIRQ_TVAL	CNTP_TVAL_EL0
#define ARM64_CNTSIRQ_CTL	CNTP_CTL_EL0
#define ARM64_CNTSIRQ_CVAL	CNTP_CVAL_EL0
#define ARM64_CNTSCHED_TVAL	CNTV_TVAL_EL0
#define ARM64_CNTSCHED_CTL	CNTV_CTL_EL0
#define ARM64_CNTSCHED_CVAL	CNTV_CVAL_EL0

#define ARM64_SPSR_VALUE	0x1c5
#define ARM64_SPSR_KERNEL	AARCH64_SPSR_EL1h
#define ARM64_SPSR_USER		AARCH64_SPSR_EL0t

#define ARM64_SCTLR_VALUE	\
	SCTLR_EL1_UCI | SCTLR_EL1_UCT | SCTLR_EL1_DZE

// #define ARM64_TCR_VALUE		0x25B5103510
// VA[55] == 0 : TCR_ELx.TBI0 determines whether address tags are used.
// VA[55] == 1 : TCR_ELx.TBI1 determines whether address tags are used.
// use 512GB VA size, 1T IPA
#define ARM64_TCR_VALUE	\
	TCR_T0SZ(39) | TCR_T1SZ(39) | TCR_IRGN0_WBWA | TCR_IRGN1_WBWA |		\
	TCR_ORGN0_WBWA | TCR_ORGN1_WBWA | TCR_SH0_INNER | TCR_SH1_INNER |	\
	TCR_TG1_4K | TCR_TG0_4K | TCR_ASID16 | TCR_TBI0

#endif
