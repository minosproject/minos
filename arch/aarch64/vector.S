#include <asm/aarch64_common.h>
#include <config/config.h>

	.section __el2_vectors, "ax"
	.balign 8

	.global __irq_handler
	.global __serror_handler
	.global arch_switch_task_sw

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

#ifdef CONFIG_HAS_VIRT
	mrs	x0, esr_el2
	str	x0, [sp, #-8]!
	mrs	x0, spsr_el2
	str	x0, [sp, #-8]!
	mrs	x0, elr_el2
	str	x0, [sp, #-8]!
#else
	mrs	x0, esr_el1
	str	x0, [sp, #-8]!
	mrs	x0, spsr_el1
	str	x0, [sp, #-8]!
	mrs	x0, elr_el1
	str	x0, [sp, #-8]!
#endif
.endm

.macro restore_gp_regs
#ifdef CONFIG_HAS_VIRT
	ldr	x0, [sp], #8
	msr	elr_el2, x0
	ldr	x0, [sp], #8
	msr	spsr_el2, x0
	ldr	x0, [sp], #8
	msr	esr_el2, x0
#else
	ldr	x0, [sp], #8
	msr	elr_el1, x0
	ldr	x0, [sp], #8
	msr	spsr_el1, x0
	ldr	x0, [sp], #8
	msr	esr_el1, x0
#endif
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
.endm

	.type arch_switch_task_sw "function"
	.cfi_startproc
arch_switch_task_sw:
	save_gp_regs

	ldr	x5, =per_cpu_percpu_current_task
	ldr	x4, =per_cpu_percpu_next_task
	mrs	x1, mpidr_el1
	ubfx	x1, x1, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ldr	x2, =percpu_offset
	ldr	x3, [x2]
	sub	x5, x5, x3
	sub	x4, x4, x3
	lsl	x1, x1, #3
	add	x2, x2, x1
	ldr	x2, [x2]
	add	x5, x5, x2
	add	x4, x4, x2
	ldr	x0, [x5]		// x0 is current running task
	ldr	x1, [x4]		// x4 is next running task

	mov	x2, sp
	str	x2, [x0]

	ldr	x2, [x1]
	mov	sp, x2

	/* current = next */
	str	x1, [x5]

	restore_gp_regs

	eret
	.cfi_endproc

	.type __serror_handler "function"
	.cfi_startproc
__serror_handler:
	save_gp_regs

	mov	x0, sp
	bl	serror_c_handler			// go to the c handler

	restore_gp_regs

	eret
	.cfi_endproc

	.type __irq_handler "function"
	.cfi_startproc
__irq_handler:
	save_gp_regs

	/* can not call sched() in irq handler */
	mov	x0, sp
	bl	irq_c_handler

	ldr	x5, =per_cpu_percpu_current_task
	ldr	x4, =per_cpu_percpu_next_task
	mrs	x1, mpidr_el1
	ubfx	x1, x1, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ldr	x2, =percpu_offset
	ldr	x3, [x2]
	sub	x5, x5, x3
	sub	x4, x4, x3
	lsl	x1, x1, #3
	add	x2, x2, x1
	ldr	x2, [x2]
	add	x5, x5, x2
	add	x4, x4, x2
	ldr	x0, [x5]		// x0 is current running task
	ldr	x1, [x4]		// x4 is next running task

	cmp	x0, x1
	beq	__switch_to_task

	stp	x4, x5, [sp, #-16]
	stp	x0, x1, [sp, #-16]
	bl	switch_to_task
	ldp	x0, x1, [sp], #16
	ldp	x4, x5, [sp], #16

	mov	x2, sp
	str	x2, [x0]		// save the stack to the stack base
	str	x1, [x5]		// current task = next task
	ldr	x2, [x1]
	mov	sp, x2		// load the sp from the next task

__switch_to_task:
	restore_gp_regs

	eret
	.cfi_endproc
