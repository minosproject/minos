#include <asm/aarch64_common.h>
#include <config/config.h>

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

	.macro find_running_vcpu
	/*
	 * find the running vcpu in this cpu
	 */
	ldr	x0, =per_cpu_running_vcpu
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
	.endm

	.type _sync_el2_handler "function"
	.cfi_startproc
_sync_el2_handler:
	stp	x2, x3, [sp, #-16]!
	stp	x0, x1, [sp, #-16]!

	//find_running_vcpu
	ldr	x0, =per_cpu_running_vcpu
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
	stp	x4, x5, [x0], #16
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
	mrs	x1, vbar_el1
	str	x1, [x0], #8
	mrs	x1, spsr_el2
	str	x1, [x0], #8
	mrs	x1, nzcv
	str	x1, [x0], #8
	mrs	x1, esr_el1
	str	x1, [x0], #8
	mrs	x1, vmpidr_el2
	str	x1, [x0], #8
	mrs	x1, sctlr_el1
	str	x1, [x0], #8
	mrs	x1, ttbr0_el1
	str	x1, [x0], #8
	mrs	x1, ttbr1_el1
	str	x1, [x0], #8
	mrs	x1, vttbr_el2
	str	x1, [x0], #8
	mrs	x1, vtcr_el2
	str	x1, [x0], #8
	mrs	x1, hcr_el2
	str	x1, [x0]
	sub	x0, x0, #344

	bl	sync_el2_handler

	//find_running_vcpu
	ldr	x0, =per_cpu_running_vcpu
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
	msr	SP_EL1, x1
	ldr	x1, [x0], #8
	msr	elr_el2, x1
	ldr	x1, [x0], #8
	msr	vbar_el1, x1
	ldr	x1, [x0], #8
	msr	spsr_el2, x1
	ldr	x1, [x0], #8
	msr	nzcv, x1
	ldr	x1, [x0], #8
	msr	esr_el1, x1
	ldr	x1, [x0], #8
	msr	vmpidr_el2, x1
	ldr	x1, [x0], #8
	msr	sctlr_el1, x1
	ldr	x1, [x0], #8
	msr	ttbr0_el1, x1
	ldr	x1, [x0], #8
	msr	ttbr1_el1, x1
	ldr	x1, [x0], #8
	msr	vttbr_el2, x1
	ldr	x1, [x0], #8
	msr	vtcr_el2, x1
	ldr	x1, [x0], #8
	msr	hcr_el2, x1
	sub	x0, x0, #352
	ldr	x1, [x0, #8]
	ldr	x0, [x0]
	eret
	.cfi_endproc
