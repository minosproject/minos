#include <asm/aarch64_common.h>
#include <config/config.h>

	.global el2_vectors

	.section __el2_vectors, "ax"
	.align 11

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
	b __serror_handler
	.balign 0x80
cxirq2:
	b __irq_handler
	.balign 0x80
cxfiq2:
	b cxfiq2
	.balign 0x80
cxserr2:
	b cxserr2
	.balign 0x80	//Lower EL using AArch64
l64sync2:
	b __serror_handler
	.balign 0x80
l64irq2:
	b __irq_handler
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

