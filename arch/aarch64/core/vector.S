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
#include <asm/asm-offset.h>
#include <asm/aarch64_reg.h>
#include <minos/task_info.h>

	.section __elx_vectors, "ax"
	.balign 8

.macro vfunc name
	.global \name
	.type \name "function"
	.cfi_startproc
	\name:
.endm

.macro vfunc_end name
	.cfi_endproc
.endm

// ARM64_TPIDR will store the pcpu data
// x18 will store the current_task
.macro PCPU_SAVE_CURRENT_TASK tmp0
	mrs	\tmp0, ARM64_TPIDR
	str	x18, [\tmp0, #PCPU_CURRENT_TASK]
.endm

.macro PCPU_LOAD_CURRENT_TASK
	mrs	x18, ARM64_TPIDR
	ldr	x18, [x18, #PCPU_CURRENT_TASK]
.endm

.macro LOAD_PCPU_STACK, tmp0
	mrs	\tmp0, ARM64_TPIDR
	ldr	\tmp0, [\tmp0, #PCPU_STACK_OFFSET]
	mov	sp, \tmp0
.endm

.macro __SAVE_GP_REGS
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
	mrs	x0, SP_EL0
	str	x0, [sp, #-8]!
	mrs	x0, ARM64_SPSR
	str	x0, [sp, #-8]!
	mrs	x0, ARM64_ELR
	str	x0, [sp, #-8]!
	dsb	nsh
.endm

.macro SAVE_GP_REGS
	stp	x29, x30, [sp, #-16]!
	__SAVE_GP_REGS
.endm

.macro SAVE_GP_REGS_SW
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

	mrs	x2, SP_EL0			// SP_EL0
	str	x2, [sp, #-8]!

	mrs	x2, daif			// pstate
	mrs	x3, nzcv
	mov	x4, #ARM64_SPSR_KERNEL
	orr	x2, x2, x3
	orr	x2, x2, x4
	str	x2, [sp, #-8]!

	str	x30, [sp, #-8]!			// pc
	dsb	nsh
.endm

.macro LOAD_GP_REGS
	ldr	x0, [sp], #8			// restore task context
	msr	ARM64_ELR, x0
	ldr	x0, [sp], #8
	msr	ARM64_SPSR, x0
	ldr	x0, [sp], #8
	msr	SP_EL0, x0
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
	dsb	nsh
.endm

vfunc __bad_mode
	__SAVE_GP_REGS
	mov	x0, sp
	mov	x1, x29
	b	bad_mode	/* will never return */
lb:
	b	lb
vfunc_end __bad_mode

vfunc arch_switch_task_sw
	SAVE_GP_REGS_SW			// x0 is current task x1 is the next task

	mov	x3, sp				// save the current stack to task
	str	x3, [x0, #TASK_STACK_OFFSET]

	LOAD_PCPU_STACK x2			// load the pcpu stack as temp stack.

	bl	switch_to_task			// switch to the new task.

	ldr	x0, [x18, #TASK_STACK_OFFSET]	// load current task's stack
	mov	sp, x0

	LOAD_GP_REGS

	eret
	isb
vfunc_end arch_switch_task_sw

vfunc exception_return
	LOAD_PCPU_STACK x1			// load percpu stack, need ensure the irq is off.

	bl	exception_return_handler	// check whether need to resched. x18 will the next task.

	ldr	x1, [x18, #TASK_STACK_OFFSET]	// load the running task's stack
	mov	sp, x1				// change to the new stack address

	ldr	x1, [sp, #8]			// load spsr
	and	x1, x1, #0x0f
	cmp	x1, #9				// whether the task will return to user
	b.eq	__do_exception_return

	mov	x0, sp
	bl	task_return_to_user

__do_exception_return:
	LOAD_GP_REGS
	eret
vfunc_end exception_return

vfunc __sync_exception_from_current_el
	SAVE_GP_REGS

	mov	x0, sp
	str	x0, [x18, #TASK_STACK_OFFSET]
	bl	sync_exception_from_current_el	// go to the c handler

	b	exception_return
vfunc_end __sync_exception_from_current_el

vfunc __sync_exception_from_lower_el
	SAVE_GP_REGS

	PCPU_LOAD_CURRENT_TASK			// x18 will be the current task.

	bl	task_exit_from_user

	mov	x0, sp
	bl	sync_exception_from_lower_el	// go to the c handler.

	mov	x0, sp
	bl      task_return_to_user

	LOAD_GP_REGS
	eret
vfunc_end __sync_exception_from_lower_el

vfunc __irq_exception_from_lower_el
	SAVE_GP_REGS

	PCPU_LOAD_CURRENT_TASK			// x18 will store the current task

	mov	x0, sp				// x0 is the gp_regs pass to irq_c_handler
	str	x0, [x18, #TASK_STACK_OFFSET]	// save the current task's stack to task
	bl	task_exit_from_user

	mov	x0, sp
	bl	irq_from_lower_el		// call the c irq handler

	b	exception_return
vfunc_end __irq_exception_from_lower_el

vfunc __irq_exception_from_current_el
	SAVE_GP_REGS

	mov	x0, sp
	str     x0, [x18, #TASK_STACK_OFFSET]	// store the stack in case this task will scheded out.
	bl	irq_from_current_el		// irq is disabled all the time

	b	exception_return
vfunc_end __irq_exception_from
