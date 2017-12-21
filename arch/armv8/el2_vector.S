#include <asm/v8_system.h>
#include <config/mvisor_config.h>

	.global el2_vectors

	.section __el2_vectors, "ax"
	.align 11

el2_vectors:
c0sync2:
	b c0sync2
	.balign 0x80
c0irq2:
	b c0irq2
	.balign 0x80
c0fiq2:
	b c0fiq2
	.balign 0x80
c0serr2:
	b c0serr2
	.balign 0x80	// Current EL with SPx
cxsync2:
	b cxsync2
	.balign 0x80
cxirq2:
	b cxirq2
	.balign 0x80
cxfiq2:
	b cxfiq2
	.balign 0x80
cxserr2:
	b cxserr2
	.balign 0x80	//Lower EL using AArch64
l64sync2:
	b	_sync_el2_handler
	.balign 0x80
l64irq2:
	b l64irq2
	.balign 0x80
l64fiq2:
	b l64fiq2
	.balign 0x80
l64serr2:
	b l64serr2
	.balign 0x80	// Lower EL using AArch32
l32sync2:
	b l32sync2
	.balign 0x80
l32irq2:
	b l32irq2
	.balign 0x80
l32fiq2:
	b l32fiq2
	.balign 0x80
l32serr2:
	b l32serr2

	.section __int_handlers, "ax"
	.balign 8

vcpu_context_table:
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0

	.balign 8

	.type _sync_el2_handler "function"
_sync_el2_handler:
	/*
	 * find the right cpu_context
	 */
	stp	x2, x3, [sp, #-16]!
	stp	x0, x1, [sp, #-16]!
	ldr	x0, = vcpu_context_table
	mrs	x1, vmpidr_el2
	ubfx	x2, x1, MPIDR_EL1_AFF1_LSB, MPIDR_EL1_AFF_WIDTH
	ubfx	x3, x1, MPIDR_EL1_AFF0_LSB, MPIDR_EL1_AFF_WIDTH
	mov	x1, #VM_MAX_VCPU
	mul	x2, x2, x1
	add	x2, x2, x3
	lsl	x1, x2, #3
	add	x0, x0, x1
	ldr	x0, [x0]		// the address of vcpu context

	ldp	x2, x3, [sp], #16	// load x0, x1
	stp	x2, x3, [sp], #16	// store x0 x1 to context
	ldp	x2, x3, [sp], #16	// load x2 x3
	stp	x2, x3, [sp], #16	// store x2, x3 to contex
	stp	x4, x5, [x0], #16
	stp	x6, x7, [x0], #16
	stp	x8, x9, [x0], #16
	stp	x10, x11, [x0], #16
	stp	x12, x13, [x0], #16
	stp	x14, x15, [x0], #16
	stp	x16, x16, [x0], #16
	stp	x17, x18, [x0], #16
	stp	x19, x20, [x0], #16
	stp	x21, x22, [x0], #16
	stp	x23, x24, [x0], #16
	stp	x25, x26, [x0], #16
	stp	x27, x28, [x0], #16
	stp	x29, x30, [x0], #16
	mrs	x1, sp_el1
	str	x1, [x0], #8
	mrs	x1, elr_el2
	str	x1, [x0], #8
	mrs	x1, vbar_el1
	str	x1, [x0], #8
	mrs	x1, spsr_el1
	str	x1, [x0], #8
	mrs	x1, nzcv
	str	x1, [x0], #8
	mrs	x1, esr_el1
	str	x1, [x0], #8
	mrs	x1, vmpidr_el2
	str	x1, [x0], #8
	mrs	x1, sctlr_el1
	str	x1, [x0]
	sub	x0, x0, #304

	bl	sync_el2_handler
	/*
	 * should never return here
	 */
loop:
	ldr	x0, =error_message
	bl	panic


error_message:
	.ascii "Some thing error in the handler"
