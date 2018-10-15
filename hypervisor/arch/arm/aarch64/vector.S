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

#include <asm/aarch64_common.h>
#include <config/config.h>

	.section __el2_vectors, "ax"
	.balign 8

	.global __irq_handler
	.global __serror_handler
	.global arch_switch_vcpu_sw

.macro save_gp_regs
	stp	x29, x30, [sp, #-16]!
	stp	x27, x28, [sp, #-16]!
	stp	x25, x26, [sp, #-16]!
	stp	x23, x24, [sp, #-16]!
	stp	x21, x22, [sp, #-16]!
	stp	x19, x20, [sp, #-16]!
	stp	x17, x18, [sp, #-16]!
	stp     x15, x16, [sp, #-16]!
	stp     x13, x14, [sp, #-16]!
	stp     x11, x12, [sp, #-16]!
	stp     x9, x10, [sp, #-16]!
	stp     x7, x8, [sp, #-16]!
	stp     x5, x6, [sp, #-16]!
	stp     x3, x4, [sp, #-16]!
	stp     x1, x2, [sp, #-16]!
	str	x0, [sp, #-8]!
	mrs	x0, esr_el2
	str	x0, [sp, #-8]!
	mrs	x0, spsr_el2
	str	x0, [sp, #-8]!
	mrs	x0, elr_el2
	str	x0, [sp, #-8]!
.endm

.macro restore_gp_regs
	ldr	x0, [sp], #8
	msr	elr_el2, x0
	ldr	x0, [sp], #8
	msr	spsr_el2, x0
	mov	x30, x0
	ldr	x0, [sp], #8
	msr	esr_el2, x0
	ldp     x0, x1, [sp], #16
	ldp     x2, x3, [sp], #16
	ldp     x4, x5, [sp], #16
	ldp     x6, x7, [sp], #16
	ldp     x8, x9, [sp], #16
	ldp     x10, x11, [sp], #16
	ldp     x12, x13, [sp], #16
	ldp     x14, x15, [sp], #16
	ldp     x16, x17, [sp], #16
	ldp     x18, x19, [sp], #16
	ldp     x20, x21, [sp], #16
	ldp     x22, x23, [sp], #16
	ldp     x24, x25, [sp], #16
	ldp     x26, x27, [sp], #16
	ldp     x28, x29, [sp], #16

	and	x30, x30, #0xf
	cmp	x30, #0x9
	bne	1f
	ldr	x30, [sp], #8
	msr	elr_el2, x30
	eret
1:
	ldr	x30, [sp], #8
.endm

	.type arch_switch_vcpu_sw "function"
	.cfi_startproc
arch_switch_vcpu_sw:
	//save_gp_regs
	stp	x29, x30, [sp, #-16]!
	stp	x27, x28, [sp, #-16]!
	stp	x25, x26, [sp, #-16]!
	stp	x23, x24, [sp, #-16]!
	stp	x21, x22, [sp, #-16]!
	stp	x19, x20, [sp, #-16]!
	stp	x17, x18, [sp, #-16]!
	stp     x15, x16, [sp, #-16]!
	stp     x13, x14, [sp, #-16]!
	stp     x11, x12, [sp, #-16]!
	stp     x9, x10, [sp, #-16]!
	stp     x7, x8, [sp, #-16]!
	stp     x5, x6, [sp, #-16]!
	stp     x3, x4, [sp, #-16]!
	stp     x1, x2, [sp, #-16]!
	str	x0, [sp, #-8]!
	mrs	x0, esr_el2
	str	x0, [sp, #-8]!

	mrs	x0, daif
	mrs	x1, nzcv
	mov	x2, #0x9
	orr	x0, x0, x1
	orr	x0, x0, x2
	str	x0, [sp, #-8]!

	str	x30, [sp, #-8]!

	ldr	x5, =per_cpu_percpu_current_vcpu
	ldr	x4, =per_cpu_percpu_next_vcpu
	mrs	x1, mpidr_el1
	ubfx	x8, x1, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ubfx	x9, x1, #MPIDR_EL1_AFF1_LSB, #MPIDR_EL1_AFF_WIDTH
	mov	x10, #CONFIG_NR_CPUS_CLUSTER0
	mul	x2, x9, x10
	add	x1, x2, x8
	ldr	x2, =percpu_offset
	ldr	x3, [x2]
	sub	x5, x5, x3
	sub	x4, x4, x3
	lsl	x1, x1, #3
	add	x2, x2, x1
	ldr	x2, [x2]
	add	x5, x5, x2
	add	x4, x4, x2
	ldr	x0, [x5]		// x0 is current running vcpu
	ldr	x1, [x4]		// x4 is next running vcpu

	mov	x2, sp
	str	x2, [x0]

	ldr	x2, [x1]
	mov	sp, x2

	str	x1, [x5]		// current = next
	dsb sy

	//restore_gp_regs
	ldr	x0, [sp], #8
	msr	elr_el2, x0
	ldr	x0, [sp], #8
	msr	spsr_el2, x0
	ldr	x0, [sp], #8
	msr	esr_el2, x0
	ldp     x0, x1, [sp], #16
	ldp     x2, x3, [sp], #16
	ldp     x4, x5, [sp], #16
	ldp     x6, x7, [sp], #16
	ldp     x8, x9, [sp], #16
	ldp     x10, x11, [sp], #16
	ldp     x12, x13, [sp], #16
	ldp     x14, x15, [sp], #16
	ldp     x16, x17, [sp], #16
	ldp     x18, x19, [sp], #16
	ldp     x20, x21, [sp], #16
	ldp     x22, x23, [sp], #16
	ldp     x24, x25, [sp], #16
	ldp     x26, x27, [sp], #16
	ldp     x28, x29, [sp], #16
	ldr	x30, [sp], #8
	eret
	.cfi_endproc

	.type __serror_handler "function"
	.cfi_startproc
__serror_handler:
	//save_gp_regs
	stp	x29, x30, [sp, #-16]!
	stp	x27, x28, [sp, #-16]!
	stp	x25, x26, [sp, #-16]!
	stp	x23, x24, [sp, #-16]!
	stp	x21, x22, [sp, #-16]!
	stp	x19, x20, [sp, #-16]!
	stp	x17, x18, [sp, #-16]!
	stp     x15, x16, [sp, #-16]!
	stp     x13, x14, [sp, #-16]!
	stp     x11, x12, [sp, #-16]!
	stp     x9, x10, [sp, #-16]!
	stp     x7, x8, [sp, #-16]!
	stp     x5, x6, [sp, #-16]!
	stp     x3, x4, [sp, #-16]!
	stp     x1, x2, [sp, #-16]!
	str	x0, [sp, #-8]!
	mrs	x0, esr_el2
	str	x0, [sp, #-8]!
	mrs	x0, spsr_el2
	str	x0, [sp, #-8]!
	mrs	x0, elr_el2
	str	x0, [sp, #-8]!

	mov	x0, sp
	bl	serror_c_handler			// go to the c handler

	//restore_gp_regs
	ldr	x0, [sp], #8
	msr	elr_el2, x0
	ldr	x0, [sp], #8
	msr	spsr_el2, x0
	ldr	x0, [sp], #8
	msr	esr_el2, x0
	ldp     x0, x1, [sp], #16
	ldp     x2, x3, [sp], #16
	ldp     x4, x5, [sp], #16
	ldp     x6, x7, [sp], #16
	ldp     x8, x9, [sp], #16
	ldp     x10, x11, [sp], #16
	ldp     x12, x13, [sp], #16
	ldp     x14, x15, [sp], #16
	ldp     x16, x17, [sp], #16
	ldp     x18, x19, [sp], #16
	ldp     x20, x21, [sp], #16
	ldp     x22, x23, [sp], #16
	ldp     x24, x25, [sp], #16
	ldp     x26, x27, [sp], #16
	ldp     x28, x29, [sp], #16
	ldr	x30, [sp], #8
	eret
	.cfi_endproc

	.type __irq_handler "function"
	.cfi_startproc
__irq_handler:
	//save_gp_regs
	stp	x29, x30, [sp, #-16]!
	stp	x27, x28, [sp, #-16]!
	stp	x25, x26, [sp, #-16]!
	stp	x23, x24, [sp, #-16]!
	stp	x21, x22, [sp, #-16]!
	stp	x19, x20, [sp, #-16]!
	stp	x17, x18, [sp, #-16]!
	stp     x15, x16, [sp, #-16]!
	stp     x13, x14, [sp, #-16]!
	stp     x11, x12, [sp, #-16]!
	stp     x9, x10, [sp, #-16]!
	stp     x7, x8, [sp, #-16]!
	stp     x5, x6, [sp, #-16]!
	stp     x3, x4, [sp, #-16]!
	stp     x1, x2, [sp, #-16]!
	str	x0, [sp, #-8]!
	mrs	x0, esr_el2
	str	x0, [sp, #-8]!
	mrs	x0, spsr_el2
	str	x0, [sp, #-8]!
	mrs	x0, elr_el2
	str	x0, [sp, #-8]!

	/* can not call sched() in irq handler */
	mov	x0, sp
	bl	irq_c_handler

	ldr	x5, =per_cpu_percpu_current_vcpu
	ldr	x4, =per_cpu_percpu_next_vcpu
	mrs	x1, mpidr_el1
	ubfx	x8, x1, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ubfx	x9, x1, #MPIDR_EL1_AFF1_LSB, #MPIDR_EL1_AFF_WIDTH
	mov	x10, #CONFIG_NR_CPUS_CLUSTER0
	mul	x2, x9, x10
	add	x1, x2, x8
	ldr	x2, =percpu_offset
	ldr	x3, [x2]
	sub	x5, x5, x3
	sub	x4, x4, x3
	lsl	x1, x1, #3
	add	x2, x2, x1
	ldr	x2, [x2]
	add	x5, x5, x2
	add	x4, x4, x2
	ldr	x0, [x5]		// x0 is current running vcpu
	ldr	x1, [x4]		// x4 is next running vcpu

	stp	x4, x5, [sp, #-16]!
	stp	x0, x1, [sp, #-16]!
	bl	switch_to_vcpu
	ldp	x0, x1, [sp], #16
	ldp	x4, x5, [sp], #16

	cmp	x0, x1
	beq	__switch_to_vcpu

	mov	x2, sp
	str	x2, [x0]		// save the stack to the stack base

	ldr	x2, [x1]
	mov	sp, x2			// load the sp from the next vcpu

	str	x1, [x5]		// current vcpu = next vcpu
	dsb sy

__switch_to_vcpu:
	//restore_gp_regs
	ldr	x0, [sp], #8
	msr	elr_el2, x0
	ldr	x0, [sp], #8
	msr	spsr_el2, x0
	ldr	x0, [sp], #8
	msr	esr_el2, x0
	ldp     x0, x1, [sp], #16
	ldp     x2, x3, [sp], #16
	ldp     x4, x5, [sp], #16
	ldp     x6, x7, [sp], #16
	ldp     x8, x9, [sp], #16
	ldp     x10, x11, [sp], #16
	ldp     x12, x13, [sp], #16
	ldp     x14, x15, [sp], #16
	ldp     x16, x17, [sp], #16
	ldp     x18, x19, [sp], #16
	ldp     x20, x21, [sp], #16
	ldp     x22, x23, [sp], #16
	ldp     x24, x25, [sp], #16
	ldp     x26, x27, [sp], #16
	ldp     x28, x29, [sp], #16
	ldr	x30, [sp], #8
	eret
	.cfi_endproc
