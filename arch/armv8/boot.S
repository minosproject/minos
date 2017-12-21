/*
 * arch/arm64/kernel/boot.S
 *
 * Created by Le Min(lemin9538@163.com)
 */

#include "include/v8_mmu.h"
#include <asm/v8_system.h>
#include "include/GICv3_aliases.h"
#include <config/mvisor_config.h>

	.section __start_up, "ax"
	.balign 4

	.global _start
	.global __code_start
	.global __code_end
	.global __vmm_vm_start
	.global __vmm_vm_end
	.type _start, "function"
_start:	
	// SCR_EL3 Secure Configuration Register
	ldr	x1, =el3_vectors
	msr	VBAR_EL3, x1

	msr	SCR_EL3, xzr
	isb

	//ICC_SRE_EL3 Interrupt Controller System Register Enable register 
	mov	x0, #15
	msr	ICC_SRE_EL3, x0
	isb

	mov	x3, #(SCR_EL3_RW | \
			SCR_EL3_SMD | \
			SCR_EL3_NS | \
			SCR_EL3_HCE)
	msr	SCR_EL3, x3
	isb

	/*
	 * init the vmpidr
	 */
	mrs	x0, MPIDR_EL1
	msr	VMPIDR_EL2, x0
	mrs	x0, MIDR_EL1
	msr	VPIDR_EL2, x0

	bl get_cpu_id
	mov	x19, x0

	msr	CPTR_EL3, xzr

	mov	x0, #0
	msr	SCTLR_EL3, x0

	.global drop_to_el2
drop_to_el2:
	adr	x1, el2_entry_aarch64
	msr	ELR_EL3, X1
	mov	x1, #(AARCH64_SPSR_EL2h | \
		AARCH64_SPSR_F  | \
		AARCH64_SPSR_I  | \
		AARCH64_SPSR_A)
	msr	SPSR_EL3, x1
	eret

	.global el2_entry_aarch64
	.type el2_entry_aarch64, "function"
el2_entry_aarch64:
	ldr	x1, =el2_vectors
	msr	VBAR_EL2, x1

	mov	x0, #7	// el1 access ICC_SRE_EL1 will trap to EL2
	msr	ICC_SRE_EL2, x0

	msr	VTTBR_EL2, xzr
	isb

	// neither EL3 nor EL2 trap floating point or accesses to CPACR
	msr	CPTR_EL2, xzr

	ldr	x0, =MVISOR_STACK_BASE
	sub	x0, x0, x19, lsl #14
	mov	sp, x0

	ldr	x0, =__bss_start
	mov	x1, #0
	ldr	x2, =__bss_end
	sub	x2, x2, x0
	bl	memset

	ldr	x0, = (HCR_EL2_HVC | \
		      HCR_EL2_TWI | \
		      HCR_EL2_TWE | \
		      HCR_EL2_TWE | \
		      HCR_EL2_RW)
	msr	hcr_el2, x0

	// enable interrupt


	b	boot_main
