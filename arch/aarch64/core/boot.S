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
#include <asm/asm-offset.h>

#define SCTLR_EL2_VALUE		0x30c51878

	.section __start_up, "ax"
	.balign 4

	.global _start
	.type _start, "function"
_start:
	/* interrupt disabled mmu/dcache/icache off */
	msr	daifset, #2
	b	do_start
	.quad   CONFIG_MINOS_ENTRY_ADDRESS	     /* Image load offset from start of RAM */
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

	adr	x1, _start			// entry address must 4K align
	and	x0, x1, #0xfff
	cbnz	x0, minos_panic

	// minos_start to __code_start is the reserve memory
	// for minos
	ldr	x3, =0xffffffffffe00000
	and	x1, x1, x3
	adr	x2, minos_start
	str	x1, [x2]

	mrs	x0, midr_el1
	mrs	x1, mpidr_el1
	msr	vpidr_el2, x0
	msr	vmpidr_el2, x1

	bl	get_cpuid			// cpuid save to x19
	mov	x19, x0

	/*
	 * store the cpuid to TPIDR_EL2
	 */
	msr	TPIDR_EL2, x0
	dsb	sy

	msr	VTTBR_EL2, xzr
	isb

	/*
	 * neither EL3 nor EL2 trap floating point or
	 * accesses to CPACR
	 */
	msr	CPTR_EL2, xzr

	/* using current EL stack register */
	msr	spsel, #1
	dsb	sy

	ldr	x1, =el2_vectors
	msr	VBAR_EL2, x1
	dsb	sy

	/* invalid the dcache and flush the tlb */
	bl	inv_dcache_all
	dsb	sy
	isb

	tlbi	alle2
	dsb	sy
	isb

	ldr	x1, =SCTLR_EL2_VALUE
	msr	SCTLR_EL2, x1
	dsb	sy
	isb

	/* setup the el2 page table */
	ldr	x1, = __el2_page_table
	msr	TTBR0_EL2, x1
	dsb	sy
	isb

	/*
	 * 0xff440c0400
	 * MT_DEVICE_NGNRNE	0
	 * MT_DEVICE_NGNRE	1
	 * MT_DEVICE_GRE	2
	 * MT_NORMAL_NC		3
	 * MT_NORMAL		4
	 * 0x00 - MT_DEVICE_NGNRNE
	 * 0x04 - MT_DEVICE_NGNRE
	 * 0x0c - MT_DEVICE_GRE
	 * 0x44 - MT_NORMAL_NC
	 * 0x0ff - MT_NORMAL
	 */
	ldr	x1, =0xff440c0400
	msr	MAIR_EL2, x1
	dsb	sy
	isb

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
	orr	x1, x1, #(1 << 8)	// IRGN0 : Normal memory, Inner Write-Back Write-Allocate Cacheable
	orr	x1, x1, #(1 << 10)	// ORGN0 : Normal memory, Outer Write-Back Write-Allocate Cacheable
	orr	x1, x1, #(3 << 12)	// Inner shareable
	orr	x1, x1, #(1 << 23)	// Reserved, res1.
	msr	TCR_EL2, x1
	dsb	sy

	/* idle task is in EL2 */
	mov	x1, #0x1c9
	msr	spsr_el2, x1
	dsb	sy

	cbnz	x19, secondary_start_up

	ldr	x0, =__minos_end
	adr	x1, minos_stack_top
	adr	x2, minos_bootmem_base
	adr	x3, minos_stack_bottom

	/* store the bootmem base */
	add	x0, x0, #4095
	mov	x4, #4095
	mvn	x5, x4
	and	x0, x0, x5
	str	x0, [x2]

	/* idle task stack need NK align */
	mov	x4, #(CONFIG_TASK_STACK_SIZE - 1)
	add	x0, x0, x4
	mvn	x5, x4
	and	x0, x0, x5

	/* store the minos stack bottom */
	str	x0, [x3]

	/* store the minos stack top */
	mov	x4, #CONFIG_TASK_STACK_SIZE
	mov	x5, #CONFIG_NR_CPUS
	mul	x5, x4, x5
	add	x0, x0, x5
	str	x0, [x1]
	dsb	sy

	/* each idle task will have 8k stack */
	sub	x0, x0, x19, lsl #CONFIG_TASK_STACK_SHIFT
	sub	x0, x0, #TASK_INFO_SIZE
	mov	x1, 0
	mov	x2, #TASK_INFO_SIZE
	bl	memset
	mov	sp, x0
	mov	x28, x0		/* task info save to x28 */

	ldr	x0, =__bss_start
	mov	x1, #0
	ldr	x2, =__bss_end
	sub	x2, x2, x0
	bl	memset
	dsb	sy

	/* map the boot memory when booting */
	bl	map_boot_mem
	dsb	sy

	ldr	x26, =mmu_on

	/* enable the mmu and disable the aligment check */
	mrs	x1, SCTLR_EL2
	orr	x1, x1, #SCTLR_ELx_M
	orr	x1, x1, #SCTLR_ELx_C
	orr	x1, x1, #SCTLR_ELx_I
	bic	x1, x1, #SCTLR_ELx_SA
	bic	x1, x1, #SCTLR_ELx_A
	msr	SCTLR_EL2, x1
	dsb	sy
	isb

	br	x26

mmu_on:
	tlbi	alle2
	dsb sy
	isb

	mov	x0, x27		// restore the dtb address
	bl	arch_main
	nop

secondary_start_up:
	/* setup the idle task stack */
	adr	x0, minos_stack_top
	ldr	x0, [x0]
	sub	x0, x0, x19, lsl #CONFIG_TASK_STACK_SHIFT
	sub	x0, x0, #TASK_INFO_SIZE
	mov	x1, 0
	mov	x2, #TASK_INFO_SIZE
	bl	memset
	mov	sp, x0
	mov	x28, x0		/* task info save to x28 */

	ldr	x1, =SCTLR_EL2_VALUE
	msr	SCTLR_EL2, x1
	dsb	sy
	isb

	/* enable the dcache and the icache */
	mrs	x1, SCTLR_EL2
	orr	x1, x1, #SCTLR_ELx_C
	orr	x1, x1, #SCTLR_ELx_I
	orr	x1, x1, #SCTLR_ELx_M
	bic	x1, x1, #SCTLR_ELx_SA
	bic	x1, x1, #SCTLR_ELx_A
	msr	SCTLR_EL2, x1
	dsb	sy
	isb

	ic	ialluis
	dsb	sy
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

minos_panic:
	nop
	b	minos_panic

	.data
	.global minos_start
	.global minos_bootmem_base
	.global minos_stack_top
	.global minos_stack_bottom
	.global minos_end
	.balignl 16, 0xdeadbeef

minos_start:		.quad	0x0
minos_bootmem_base:	.quad	0x0
minos_stack_top:	.quad	0x0
minos_stack_bottom:	.quad	0x0
minos_end:		.quad	0x0
