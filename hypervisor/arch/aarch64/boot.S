/*
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

#define SCTLR_EL2_VALUE		0x30c51878

	.section __start_up, "ax"
	.balign 4

	.global _start
	.type _start, "function"
_start:
	/* interrupt disabled mmu/dcache/icache off */
	msr	daifset, #2
	b	do_start
	.quad   CONFIG_MINOS_START_ADDRESS	     /* Image load offset from start of RAM */
        .quad   __code_end - _start		     /* reserved */
        .quad   0				     /* reserved */
        .quad   0				     /* reserved */
        .quad   0				     /* reserved */
        .quad   0				     /* reserved */
        .byte   0x41				     /* Magic number, "ARM\x64" */
        .byte   0x52
        .byte   0x4d
        .byte   0x64
        .long   0x0

do_start:
	mov	x27, x0				// save the dtb blob to x27
	mrs	x0, midr_el1
	mrs	x1, mpidr_el1
	msr	vpidr_el2, x0
	msr	vmpidr_el2, x1

	bl	smp_processor_id		// cpuid save to x19
	mov	x19, x0

	ldr	x1, =el2_vectors
	msr	VBAR_EL2, x1

	mov	x0, #15
	msr	ICC_SRE_EL2, x0
	isb

	msr	ICC_SRE_EL1, x0
	isb

	msr	VTTBR_EL2, xzr
	isb

	// neither EL3 nor EL2 trap floating point or accesses to CPACR
	msr	CPTR_EL2, xzr

	/* each idle task will have 16k stack */
	ldr	x0, =__el2_stack_end
	sub	x0, x0, x19, lsl #14
	mov	sp, x0

	/* invalid the dcache and flush the tlb */
	bl	inv_unified_dcache
	tlbi	alle2
	isb

	ldr	x1, =SCTLR_EL2_VALUE
	msr	SCTLR_EL2, x1

	/* setup the el2 page table */
	ldr	x1, = __el2_ttb0_pgd
	msr	TTBR0_EL2, x1

	mov	x1, #0xff44
	movk	x1, #4, lsl #16
	msr	MAIR_EL2, x1

	/* get the physical address range */
	mrs	x0, ID_AA64MMFR0_EL1
	and	x0, x0, #0xf
	mov	x2, x0, lsl #16

	/* config the TCR_EL2 */
	mov	x1, #0x80800000
	ldr	x3, =0xfff8ffff
	and	x1, x1, x3
	orr	x1, x1, x2
	orr	x1, x1, #0x10		// VA 48 bit address range and translation start at lvl0
	orr	x1, x1, #(1 << 8)	// IRGN0 : Normal memory, Inner Write-Back Read-Allocate Write-Allocate Cacheable
	orr	x1, x1, #(1 << 10)	// ORGN0
	orr	x1, x1, #(3 << 12)	// Inner shareable
	orr	x1, x1, #(1 << 23)
	msr	TCR_EL2, x1
	isb

	mov	x1, #0x1c9
	msr	spsr_el2, x1
	isb

	cbnz	x19, secondary_start_up

	/*
	 * map the code memory VA->PA, if need to using
	 * dcache need to enable the MMU, first clear
	 * the page table
	 */
	mov	x1, #0
	mov	x2, #4096
	ldr	x20, = __el2_ttb0_pgd
	mov	x0, x20
	bl	memset
	ldr	x21, = __el2_ttb0_pud
	mov	x0, x21
	mov	x1, #0
	mov	x2, #4096
	bl	memset
	ldr	x23, = __el2_ttb0_pmd_code
	mov	x0, x23
	mov	x1, #0
	mov	x2, #4096
	bl	memset
	ldr	x24, = __el2_ttb0_pmd_io
	mov	x0, x24
	mov	x1, #0
	mov	x2, #4096
	bl	memset

	ldr	x7, =0x1fffff
	ldr	x8, =0xffffffffffe00000

	mov	x0, x20
	mov	x1, x21
	ldr	x2, = CONFIG_MINOS_START_ADDRESS
	ldr	x3, = CONFIG_MINOS_START_ADDRESS
	ldr	x4, = CONFIG_MINOS_RAM_SIZE
	add	x6, x2, x4
	add	x6, x6, x7
	and	x2, x2, x8
	and	x3, x3, x8
	and	x6, x6, x8
	sub	x4, x6, x2	//get the real size in 2M align
	bl	buid_l0_table

	mov	x0, x21
	mov	x1, x23
	bl	build_l1_table

	mov	x0, 0
	mov	x5, #(TT_S1_ATTR_BLOCK | \
		(1 << TT_S1_ATTR_MATTR_LSB) | \
		TT_S1_ATTR_NS | \
		TT_S1_ATTR_AP_RW_ANY | \
		TT_S1_ATTR_SH_INNER | \
		TT_S1_ATTR_AF | \
		TT_S1_ATTR_nG)
	bl	build_l2_table
	dsb	ish

	/* map the uart io space for early uart print */
	ldr	x2, = CONFIG_UART_BASE
	ldr	x3, = CONFIG_UART_BASE
	ldr	x4, = CONFIG_UART_IO_SIZE
	add	x6, x2, x4
	add	x6, x6, x7
	and	x2, x2, x8
	and	x3, x3, x8
	and	x6, x6, x8
	sub	x4, x6, x2	//get the real size in 2M align

	mov	x0, x21
	mov	x1, x24
	bl	build_l1_table

	mov	x0, 0
	mov	x5, #(TT_S1_ATTR_BLOCK | \
		(2 << TT_S1_ATTR_MATTR_LSB) | \
		TT_S1_ATTR_NS | \
		TT_S1_ATTR_AP_RW_ANY | \
		TT_S1_ATTR_AF | \
		TT_S1_ATTR_nG)
	bl	build_l2_table
	dsb	ish

	/* enable the mmu and disable the aligment check */
	mrs	x1, SCTLR_EL2
	orr	x1, x1, #SCTLR_ELx_M
	bic	x1, x1, #SCTLR_ELx_A
	bic	x1, x1, #SCTLR_ELx_SA
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
	dsb	sy
	isb

	ldr	x0, =__bss_start
	mov	x1, #0
	ldr	x2, =__bss_end
	sub	x2, x2, x0
	bl	memset

	/* init the uart */
	bl	serial_init
	dsb	sy

	mov	x0, x27		//restore the dtb address
	bl	boot_main
	nop

secondary_start_up:
	ldr	x1, =SCTLR_EL2_VALUE
	msr	SCTLR_EL2, x1
	dsb	sy
	isb

	/* enable the dcache and the icache */
	mrs	x1, SCTLR_EL2
	orr	x1, x1, #SCTLR_ELx_C
	orr	x1, x1, #SCTLR_ELx_I
	bic	x1, x1, #SCTLR_ELx_A
	bic	x1, x1, #SCTLR_ELx_SA
	orr	x1, x1, #SCTLR_ELx_M
	msr	SCTLR_EL2, x1
	dsb	ish
	ic	ialluis
	dsb	ish
	isb

	ldr	x1, =__smp_affinity_id
	add	x1, x1, x19, lsl #3
	mrs	x2, MPIDR_EL1
	ldr	x4, =0x000000ff00ffffff
	and	x2, x2, x4
	str	x2, [x1]

	dsb	sy
	isb

	/* here wait for boot cpu finish tht init work */
	bl	boot_secondary
	nop

buid_l0_table:
	/*
	 * x0 : l0 table base
	 * x1 : l1 table base
	 * x2 : the virtual address
	 * x3 : the physical address
	 * x4 : map size
	 * only map one entry (512G) at boot time
	 */
	ubfx	x11, x2, #39, #9
	orr	x12, x1, #TT_S1_ATTR_TABLE
	str	x12, [x0, x11, lsl #3]
	ret

build_l1_table:
	/*
	 * x0 : l1 table base
	 * x1 : l2 table base
	 * x2 : the virtual address
	 * x3 : the physical address
	 * x4 : map size
	 * only map one entry (4G) at boot time
	 */
	ubfx	x11, x2, #30, #9
	orr	x12, x1, #TT_S1_ATTR_TABLE
	str	x12, [x0, x11, lsl #3]
	ret

build_l2_table:
	/*
	 * x0 - 0
	 * x1 - level 2 table address
	 * x2 - the virtual address
	 * x3 - the physical address
	 * x4 - the size
	 * x5 - the entry attribute
	 * 1G region, assume all the minos memory
	 * are in the same 1G region, other CASE
	 * TBD and get the index in the page table
	 * map as 2M block
	 */
	and	x11, x2, #0x3fffffff
	ubfx	x11, x11, #21, #17
	add	x1, x1, x11, lsl #3	// the base address in the page table

	bic	x3, x3, #0x1fffff
	bic	x3, x3, #0xffff000000000000

	orr	x3, x3, x5
loop:
	cbz	x4, exit_loop
	str	x3, [x1]
	sub	x4, x4, #0x200000
	add	x1, x1, #8
	add	x3, x3, #0x200000
	b loop
exit_loop:
	ret
