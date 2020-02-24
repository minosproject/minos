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
	.global __sync_handler
	.global arch_switch_task_sw

arg0		.req	x0
arg1		.req	x1
arg2		.req	x2
arg3		.req	x3
arg4		.req	x4
arg5		.req	x5
arg6		.req	x6
arg7		.req	x7

cpuid		.req	x23
ct_addr		.req	x24	/* current task address */
nt_addr		.req	x25	/* next task address */
ct		.req	x26	/* current task */
nt		.req	x27	/* next task */


	.type arch_switch_task_sw "function"
	.cfi_startproc
arch_switch_task_sw:
	stp	x29, x30, [sp, #-16]!	/* save the register to current stack */
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
	dsb	sy

	mrs	x0, daif
	mrs	x1, nzcv
	mov	x2, #0x9
	orr	x0, x0, x1
	orr	x0, x0, x2
	str	x0, [sp, #-8]!

	str	x30, [sp, #-8]!
	dsb	sy

	mrs	arg4, MPIDR_EL1		/* get the cpuid */
#ifdef	CONFIG_MPIDR_SHIFT
	ubfx    arg1, arg4, #MPIDR_EL1_AFF1_LSB, #MPIDR_EL1_AFF_WIDTH
	ubfx    arg2, arg4, #MPIDR_EL1_AFF2_LSB, #MPIDR_EL1_AFF_WIDTH
#else
	ubfx    arg1, arg4, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ubfx    arg2, arg4, #MPIDR_EL1_AFF1_LSB, #MPIDR_EL1_AFF_WIDTH
#endif
	mov	arg3, #CONFIG_NR_CPUS_CLUSTER0
	mul	arg2, arg2, arg3
	add	cpuid, arg2, arg1

	lsl	arg1, cpuid, #3
	ldr	arg5, =__current_tasks	/* get the next task */
	dsb	ld
	add	ct_addr, arg5, arg1
	ldr	ct, [ct_addr]
	dsb	ld

	ldr	arg5, =__next_tasks
	dsb	ld
	add	nt_addr, arg5, arg1
	ldr	nt, [nt_addr]

	mov	arg2, sp		/* save the current stack to task */
	str	arg2, [ct]
	dsb	st

	mov	arg0, ct
	mov	arg1, nt
	ldr	arg2, [nt]		/* load the stack of next stack */
	mov	sp, arg2
	dsb	sy

	bl	switch_to_task
	dsb	sy

	str	nt, [ct_addr]		/* current = next */
	dsb	st

	ldr	x0, [sp], #8		/* restore the register */
	msr	elr_el2, x0
	ldr	x0, [sp], #8
	msr	spsr_el2, x0
	ldr	x0, [sp], #8
	msr	esr_el2, x0
	isb
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
	dsb	sy
	eret
	.cfi_endproc

	.type __sync_handler "function"
	.cfi_startproc
__sync_handler:
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
	dsb	sy

	mov	x0, sp
	bl	sync_c_handler			// go to the c handler
	dsb	sy

	//restore_gp_regs
	ldr	x0, [sp], #8
	msr	elr_el2, x0
	ldr	x0, [sp], #8
	msr	spsr_el2, x0
	ldr	x0, [sp], #8
	msr	esr_el2, x0
	isb
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
	dsb	sy
	eret
	.cfi_endproc

	.type __irq_handler "function"
	.cfi_startproc
__irq_handler:
	stp	x29, x30, [sp, #-16]!	/* save the register for current task */
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
	dsb	sy

	mov	arg0, sp
	bl	irq_c_handler		/* call the c irq handler */
	nop

	mrs	arg4, MPIDR_EL1		/* get the cpuid */
#ifdef	CONFIG_MPIDR_SHIFT
	ubfx    arg1, arg4, #MPIDR_EL1_AFF1_LSB, #MPIDR_EL1_AFF_WIDTH
	ubfx    arg2, arg4, #MPIDR_EL1_AFF2_LSB, #MPIDR_EL1_AFF_WIDTH
#else
	ubfx    arg1, arg4, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ubfx    arg2, arg4, #MPIDR_EL1_AFF1_LSB, #MPIDR_EL1_AFF_WIDTH
#endif
	mov	arg3, #CONFIG_NR_CPUS_CLUSTER0
	mul	arg2, arg2, arg3
	add	cpuid, arg2, arg1

	ldr	arg5, =__current_tasks
	dsb	ld
	lsl	arg1, cpuid, #3
	add	ct_addr, arg5, arg1
	ldr	ct, [ct_addr]
	dsb	ld	

	ldr	arg1, =__irq_stack_end	/* get the irq stack of the current cpu */
	sub	arg1, arg1, cpuid, lsl #CONFIG_TASK_STACK_SHIFT

	mov	arg0, sp
	str	arg0, [ct]		/* store the current task's stack */
	dsb	st

	mov	sp, arg1
	mov	arg0, ct
	bl	irq_return_handler	/* to check whether need switch to a new task */

	ldr	arg5, =__next_tasks	/* check whether need to switch to a new task */
	dsb	ld
	lsl	arg1, cpuid, #3
	add	nt_addr, arg5, arg1
	ldr	nt, [nt_addr]
	dsb	ld

	ldr	arg2, [ct]		/* load the stack of the current task */
	dsb	ld

	cmp	ct, nt
	beq	__switch_to_task

	ldr	arg2, [nt]
	str	nt, [ct_addr]		/* current task = next task */
	dsb 	sy

__switch_to_task:
	mov	sp, arg2		/* load the task's stack */

	ldr	x0, [sp], #8
	msr	elr_el2, x0
	ldr	x0, [sp], #8
	msr	spsr_el2, x0
	ldr	x0, [sp], #8
	msr	esr_el2, x0
	isb
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
	dsb	sy
	eret
	.cfi_endproc
