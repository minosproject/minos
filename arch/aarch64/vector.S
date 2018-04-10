#include <asm/aarch64_common.h>
#include <config/config.h>

	.section __el2_vectors, "ax"
	.balign 8

	.global exception_from_el1_handler
	.global exception_from_el2_handler

	.type exception_from_el1_handler "function"
	.cfi_startproc
exception_from_el1_handler:
	stp	x2, x3, [sp, #-16]!
	stp	x0, x1, [sp, #-16]!

	//find_running_vcpu
	ldr	x0, =per_cpu_current_vcpu
	mrs	x1, mpidr_el1
	ubfx	x1, x1, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ldr	x2, =percpu_offset
	ldr	x3, [x2]
	sub	x0, x0, x3
	lsl	x1, x1, #3
	add	x2, x2, x1
	ldr	x2, [x2]
	add	x0, x0, x2		// x0 is the running vcpu
	ldr	x0, [x0]

	ldp	x2, x3, [sp], #16	// load x0, x1
	stp	x2, x3, [x0], #16	// store x0 x1 to context

	ldp	x2, x3, [sp], #16	// load x2 x3
	stp	x2, x3, [x0], #16	// store x2, x3 to contex

	/* x4 and x5 is stored at vector entry */
	ldp	x2, x3, [sp], #16	//load x4, x5
	stp	x2, x3, [x0], #16	//store x4, x5

	stp	x6, x7, [x0], #16
	stp	x8, x9, [x0], #16
	stp	x10, x11, [x0], #16
	stp	x12, x13, [x0], #16
	stp	x14, x15, [x0], #16
	stp	x16, x17, [x0], #16
	stp	x18, x19, [x0], #16
	stp	x20, x21, [x0], #16
	stp	x22, x23, [x0], #16
	stp	x24, x25, [x0], #16
	stp	x26, x27, [x0], #16
	stp	x28, x29, [x0], #16
	str	x30, [x0], #8
	mrs	x1, sp_el1
	str	x1, [x0], #8
	mrs	x1, elr_el2
	str	x1, [x0], #8
	mrs	x1, spsr_el2
	str	x1, [x0], #8
	mrs	x1, nzcv
	str	x1, [x0], #8
	mrs	x1, esr_el2
	str	x1, [x0], #8
	sub	x0, x0, #288

	blr	x4

	.global return_to_el1
return_to_el1:
	ldr	x5, =per_cpu_current_vcpu
	ldr	x4, =per_cpu_next_vcpu
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
	ldr	x0, [x5]		// x0 is current running vcpu
	ldr	x1, [x4]		// x4 is next running vcpu

	stp	x4, x5, [sp, #-16]!	// save x4 x5 and x0 x1 for use
	stp	x0, x1, [sp, #-16]!
	bl	switch_to_vcpu

	ldp	x0, x1, [sp], #16	// load x0, x1
	ldp	x4, x5, [sp], #16	// load x4, x5

	cmp	x0, x1
	beq	exit_el2_exception_handler
	str	x1, [x5]		// curr_vcpu = next_vcpu
	mov	x0, x1

	.global exit_el2_exception_handler
exit_el2_exception_handler:
	add	x0, x0, #16
	ldp	x2, x3, [x0], #16
	ldp	x4, x5, [x0], #16
	ldp	x6, x7, [x0], #16
	ldp	x8, x9, [x0], #16
	ldp	x10, x11, [x0], #16
	ldp	x12, x13, [x0], #16
	ldp	x14, x15, [x0], #16
	ldp	x16, x17, [x0], #16
	ldp	x18, x19, [x0], #16
	ldp	x20, x21, [x0], #16
	ldp	x22, x23, [x0], #16
	ldp	x24, x25, [x0], #16
	ldp	x26, x27, [x0], #16
	ldp	x28, x29, [x0], #16
	ldr	x30, [x0], #8
	ldr	x1, [x0], #8
	msr	sp_el1, x1
	ldr	x1, [x0], #8
	msr	elr_el2, x1
	ldr	x1, [x0], #8
	msr	spsr_el2, x1
	ldr	x1, [x0], #8
	msr	nzcv, x1
	ldr	x1, [x0], #8
	msr	esr_el2, x1
	sub	x0, x0, #288
	ldr	x1, [x0, #8]
	ldr	x0, [x0]
	isb
	eret
	.cfi_endproc

	.type exception_from_el2_handler "function"
	.cfi_startproc
exception_from_el2_handler:
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

	/*
	 * this is for stack alignment, sp must
	 * 16bytes aligment since printf function
	 * will access SMID register
	 */
	sub	sp, sp, #8
	blr     x29
	add	sp, sp, #8

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
