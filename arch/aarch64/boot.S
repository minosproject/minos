/*
 * Copyright (C) 2013 ARM Ltd.
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
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

#include <asm/asm_mmu.h>
#include <asm/aarch64_common.h>
#include <asm/gic_reg.h>
#include <config/config.h>

	.section __start_up, "ax"
	.balign 4

	.global _start
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

	mov	x0, #15
	msr	ICC_SRE_EL2, x0
	isb
	msr	ICC_SRE_EL1, X0

	/*
	 * init the vmpidr
	 */
	mrs	x0, MPIDR_EL1
	msr	VMPIDR_EL2, x0
	mrs	x0, MIDR_EL1
	msr	VPIDR_EL2, x0

	/* get the cpu id to x19 */
	mrs	x19, MPIDR_EL1
	ubfx	x19, x19, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH

	msr	CPTR_EL3, xzr

	mov	x0, #0
	msr	SCTLR_EL3, x0

	ldr	x0, =__el3_stack_end
	sub	x0, x0, x19, lsl #12
	mov	sp, x0

	/* do the gic setting for el3 */
	bl	gic_init_el3

	cbnz	x19, drop_to_el2

	/* do other init things */

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

	mov	x0, #15	// el1 access ICC_SRE_EL1 will trap to EL2
	msr	ICC_SRE_EL2, x0

	msr	VTTBR_EL2, xzr
	isb

	// neither EL3 nor EL2 trap floating point or accesses to CPACR
	msr	CPTR_EL2, xzr

	ldr	x0, =__el2_stack_end
	sub	x0, x0, x19, lsl #13
	mov	sp, x0

	/* invalid the dcache and flush the tlb */
	bl	inv_unified_dcache
	tlbi	alle1
	isb

	/* setup the el2 page table */
	ldr	x1, = __el2_ttb0_l0
	msr	TTBR0_EL2, x1

	mov	x1, #0xff44
	movk	x1, #4, lsl #16
	msr	MAIR_EL2, x1

	/*
	 * get the physical address range
	 */
	mrs	x0, ID_AA64MMFR0_EL1
	and	x0, x0, #0xf
	mov	x2, x0, lsl #16

	/* config the TCR_EL2 */
	mrs	x1, TCR_EL2
	ldr	x3, =0xfff8ffff
	and	x1, x1, x3
	orr	x1, x1, x2
	orr	x1, x1, #0x10		// VA 48 bit address range and translation start at lvl0
	orr	x1, x1, #(1 << 8)	// IRGN0 : Normal memory, Inner Write-Back Read-Allocate Write-Allocate Cacheable
	orr	x1, x1, #(1 << 10)	// ORGN0
	orr	x1, x1, #(3 << 12)	// Inner shareable
	orr	x1, x1, #(1 << 23)
	msr	TCR_EL2, x1

	mov	x1, #0x1c9
	msr	spsr_el2, x1
	isb

	cbnz	x19, secondary_start_up

	/*
	 * map the code memory VA->PA, if need to using
	 * dcache need to enable the MMU
	 */
	ldr	x20, = __el2_ttb0_l0
	mov	x0, x21
	mov	x1, #0
	mov	x2, #4096
	bl	memset

	ldr	x21, = __el2_ttb0_l1
	mov	x0, x21
	mov	x1, #0
	mov	x2, #4096
	bl	memset

	ldr	x22, = __el2_ttb0_l2_code
	mov	x0, x22
	mov	x1, #0
	mov	x2, #4096
	bl	memset

	mov	x0, x22
	ldr	x1, = CONFIG_MINOS_START_ADDRESS
	ldr	x2, = CONFIG_MINOS_START_ADDRESS
	ldr	x3, = CONFIG_MINOS_RAM_SIZE

	/*
	 * set upt the level 0 page table
	 */
	ubfx	x23, x1, #39, #9
	orr	x4, x21, #TT_S1_ATTR_TABLE
	str	x4, [x20, x23, lsl #3]

	/*
	 * set up the level 1 page table assume
	 * the code size is less than 1G
	 */
	ubfx	x23, x1, #30, #9
	orr	x4, x22, #TT_S1_ATTR_TABLE
	str	x4, [x21, x23, lsl #3]

	/*
	 * set up the level 2 page table
	 */
	bl	map_memory
	dsb	ish

	/*
	 * enable the mmu and disable the aligment check
	 */
	mrs	x1, SCTLR_EL2
	orr	x1, x1, #SCTLR_ELx_M
	bic	x1, x1, #SCTLR_ELx_A
	msr	SCTLR_EL2, x1
	isb

	dsb	ish
	ic	ialluis
	dsb	ish
	isb

	/* enable the dcache and the icache */
	mrs	x1, SCTLR_EL2
	orr	x1, x1, #SCTLR_ELx_C
	orr	x1, x1, #SCTLR_ELx_I
	msr	SCTLR_EL2, x1
	isb

	ldr	x0, =__bss_start
	mov	x1, #0
	ldr	x2, =__bss_end
	sub	x2, x2, x0
	bl	memset

	bl	boot_main
	nop

secondary_start_up:
	dsb	sy
	isb

	bl	get_cpu_id
	ldr	x1, =__smp_hoding_pen
	add	x1, x1, x0, lsl #3
	mrs	x2, MPIDR_EL1
	ldr	x4, =0x000000ff00ffffff
	and	x2, x2, x4

wait_loop:
	ldr	x3, [x1]
	sub	x3, x3, x2
	cbnz	x3, wait_loop

	dsb	sy
	isb

	/* enable the dcache and the icache */
	mrs	x1, SCTLR_EL2
	orr	x1, x1, #SCTLR_ELx_C
	orr	x1, x1, #SCTLR_ELx_I
	bic	x1, x1, #SCTLR_ELx_A
	orr	x1, x1, #SCTLR_ELx_M
	msr	SCTLR_EL2, x1
	isb

	/* here wait for boot cpu finish tht init work */
	bl	boot_secondary
	nop

	/* x0 - level 2 table address
	   x1 - the virtual address
	   x2 - the physical address
	   x3 - the size */
map_memory:
	/* check aligment */
	ldr	x4, =0xffffffffffe00000
	and	x1, x1, x4
	and	x2, x2, x4
	ldr	x7, =0x1fffff
	add	x3, x3, x7
	and	x3, x3, x4

	/* 1G region, assume all the minos memory
	   are in the same 1G region, other CASE
	   TBD and get the index in the page table */
	and	x5, x1, #0x3fffffff
	//lsr	x5, x5, #21
	ubfx	x5, x5, #21, #17
	add	x0, x0, x5, lsl #3	// the base address in the page table

	/* 4K 2M block */
	bic	x2, x2, #0x1fffff
	bic	x2, x2, #0xffff000000000000

	mov	x6, #(TT_S1_ATTR_BLOCK | \
		(1 << TT_S1_ATTR_MATTR_LSB) | \
		TT_S1_ATTR_NS | \
		TT_S1_ATTR_AP_RW_PL1 | \
		TT_S1_ATTR_SH_INNER | \
		TT_S1_ATTR_AF | \
		TT_S1_ATTR_nG)

	orr	x2, x2, x6

	/* begin the loop */
loop:
	cbz	x3, exit_loop
	str	x2, [x0]
	sub	x3, x3, #0x200000
	add	x0, x0, #8
	add	x2, x2, #0x200000
	b loop

exit_loop:
	ret
