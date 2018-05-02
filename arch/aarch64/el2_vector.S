#include <asm/aarch64_common.h>
#include <config/config.h>

	.global el2_vectors

	.section __el2_vectors, "ax"
	.align 11

	.macro EXCEPTION_FROM_EL1 func
	stp 	x4, x5, [sp, #-16]!
	ldr 	x4, =\func
	b	exception_from_el1_handler
	.endm

	.macro EXCEPTION_FROM_EL2 func
	stp	x29, x30, [sp, #-16]!
	ldr	x29, =\func
	b	exception_from_el2_handler
	.endm

el2_vectors:
c0sync2:		// Current EL with SP0
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
	EXCEPTION_FROM_EL2 SError_from_el2_handler
	.balign 0x80
cxirq2:
	EXCEPTION_FROM_EL2 IRQ_from_el2_handler
	.balign 0x80
cxfiq2:
	b cxfiq2
	.balign 0x80
cxserr2:
	b cxserr2
	.balign 0x80	//Lower EL using AArch64
l64sync2:
	EXCEPTION_FROM_EL1 SError_from_el1_handler
	.balign 0x80
l64irq2:
	EXCEPTION_FROM_EL1 IRQ_from_el1_handler
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

