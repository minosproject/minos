/*
 * arch/arm64/kernel/boot.S
 *
 * Created by Le Min(lemin9538@163.com)
 */
	.text
#include "include/v8_mmu.h"
#include "include/v8_system.h"
#include "include/GICv3_aliases.h"

.global _start	
_start:	
	// SCR_EL3 Secure Configuration Register
	msr SCR_EL3, xzr
	isb

	//ICC_SRE_EL3 Interrupt Controller System Register Enable register 
	mov x0, #15
	msr ICC_SRE_EL3, x0
	isb
	msr ICC_SRE_EL1, x0

	//EL1 and EL0 is in non-secure state
	mov x3, #(SCR_EL3_RW | SCR_EL3_SMD | SCR_EL3_NS)
	msr SCR_EL3, x3
	isb

	mov x0, #15
	msr ICC_SRE_EL2, x0
	isb
	msr ICC_SRE_EL1, x0

	//
	// no traps or vm modification from the Hypervisor, EL1 is AArch64
	// HCR_EL2 register is to set the HV behavior
	//
	mov x2, #HCR_EL2_RW
	msr HCR_EL2, x2

	// ensure VTTBR_EL2 is properly initialised
	msr VTTBR_EL2, xzr
	
	mrs x0, MPIDR_EL1
	msr VMPIDR_EL2, x0
	mrs x0, MIDR_EL1
	msr VPIDR_EL2, x0

	bl GetCPUID
	mov x19, x0

	//
	// neither EL3 nor EL2 trap floating point or accesses to CPACR
	//
	msr CPTR_EL3, xzr
	msr CPTR_EL2, xzr

	//
	// sctlr can control the cache
	//
	mov x0, #0
	msr SCTLR_EL3, x0
	msr SCTLR_EL2, x0
	msr SCTLR_EL1, x0

#if 1
	mov x0, #((1 << 0) | \
              (1 << 1) | \
              (1 << 4) | \
              (1 << 5) | \
              (1 << 6))

	msr ACTLR_EL3, x0
	msr ACTLR_EL2, x0

	mrs x0, S3_1_c15_c2_1  // Read EL1 CPU Extended Control Register
	orr x0, x0, #(1 << 6)  // Set the SMPEN bit
	msr S3_1_c15_c2_1, x0  // Write EL1 CPU Extended Control Register
#endif

loop:
	b loop
