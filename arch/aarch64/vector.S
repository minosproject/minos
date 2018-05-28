#include <asm/aarch64_common.h>
#include <config/config.h>

	.section __el2_vectors, "ax"
	.balign 8

	.global __irq_handler
	.global __serror_handler

	.type __serror_handler "function"
	.cfi_startproc
__serror_handler:
	stp 	x30, x29, [sp, #-16]!
	stp 	x28, x27, [sp, #-16]!
	stp 	x26, x25, [sp, #-16]!
	stp 	x24, x23, [sp, #-16]!
	stp 	x22, x21, [sp, #-16]!
	stp 	x20, x19, [sp, #-16]!
	stp 	x18, x17, [sp, #-16]!
	stp 	x16, x15, [sp, #-16]!
	stp 	x14, x13, [sp, #-16]!
	stp 	x12, x11, [sp, #-16]!
	stp 	x10, x9, [sp, #-16]!
	stp 	x8, x7, [sp, #-16]!
	stp 	x6, x5, [sp, #-16]!
	stp 	x4, x3, [sp, #-16]!
	stp 	x2, x1, [sp, #-16]!
	str	x0, [sp, #-8]!

#ifndef	CONFIG_HAS_VIRT
	mrs	x0, esr_el1
	str	x0, [sp, #-8]!
	mrs	x0, spsr_el1
	str	x0, [sp, #-8]!
	mrs	x0, elr_el1
	str	x0, [sp, #-8]!
#else
	mrs	x0, esr_el2
	str	x0, [sp, #-8]!
	mrs	x0, spsr_el2
	str	x0, [sp, #-8]!
	mrs	x0, elr_el2
	str	x0, [sp, #-8]!
#endif
	mov	x0, sp

	/* sched() will be called when excption is
	   from el1, so need to store the sp to the
	   task's stack base */
	ldr	x5, =per_cpu_percpu_current_task
	mrs	x1, mpidr_el1
	ubfx	x1, x1, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ldr	x2, =percpu_offset
	ldr	x3, [x2]
	sub	x5, x5, x3
	lsl	x1, x1, #3
	add	x2, x2, x1
	ldr	x2, [x2]
	add	x5, x5, x2
	ldr	x1, [x5]		// x5 is current running vcpu
	str	x0, [x1]		// store sp to stack base

	bl	serror_c_handler			// go to the c handler

	/* default there will no sched() happened when
	   hanlde serror error from el1 unless called
	   sched() to drop cpu, so directly use the sp
	   register when return to EL1 */
	ldr	x0, [sp], #8
	msr	elr_el2, x0
	ldr	x0, [sp], #8
	msr	spsr_el2, x0
	ldr	x0, [sp], #8
	msr	esr_el2, x0
	ldp	x0, x1, [sp], #16
	ldp	x2, x3, [sp], #16
	ldp	x4, x5, [sp], #16
	ldp	x6, x7, [sp], #16
	ldp	x8, x9, [sp], #16
	ldp	x10, x11, [sp], #16
	ldp	x12, x13, [sp], #16
	ldp	x14, x15, [sp], #16
	ldp	x16, x17, [sp], #16
	ldp	x18, x19, [sp], #16
	ldp	x20, x21, [sp], #16
	ldp	x22, x23, [sp], #16
	ldp	x24, x25, [sp], #16
	ldp	x26, x27, [sp], #16
	ldp	x28, x29, [sp], #16
	ldr	x30, [sp], #8
	eret
	.cfi_endproc

	.type __irq_handler "function"
	.cfi_startproc
__irq_handler:
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

	ldr	x5, =per_cpu_percpu_current_task
	mrs	x1, mpidr_el1
	ubfx	x1, x1, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ldr	x2, =percpu_offset
	ldr	x3, [x2]
	sub	x5, x5, x3
	lsl	x1, x1, #3
	add	x2, x2, x1
	ldr	x2, [x2]
	add	x5, x5, x2
	ldr	x1, [x5]		// x5 is current running vcpu
	str	x0, [x1]		// store sp to stack base

#ifndef	CONFIG_HAS_VIRT
	mrs	x0, esr_el1
	str	x0, [sp, #-8]!
	mrs	x0, spsr_el1
	str	x0, [sp, #-8]!
	mrs	x0, elr_el1
	str	x0, [sp, #-8]!
#else
	mrs	x0, esr_el2
	str	x0, [sp, #-8]!
	mrs	x0, spsr_el2
	str	x0, [sp, #-8]!
	mrs	x0, elr_el2
	str	x0, [sp, #-8]!
#endif
	bl	irq_handler

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
