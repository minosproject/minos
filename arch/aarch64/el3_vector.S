	.global el3_vectors

	.section __el3_vectors, "ax"
	.align 10

el3_vectors:
c0sync3: b c0sync3

	.balign 0x80
c0irq3:
	b c0irq3

	.balign 0x80
c0fiq3:
	b c0fiq3

	.balign 0x80
c0serr3:
	b c0serr3

//
// Current EL with SPx
//
    .balign 0x80
cxsync3: b cxsync3

    .balign 0x80
cxirq3: b cxirq3

    .balign 0x80
cxfiq3: b cxfiq3

    .balign 0x80
cxserr3: b cxserr3

//
// Lower EL using AArch64
//
    .balign 0x80
l64sync3: b l64sync3

    .balign 0x80
l64irq3: b l64irq3

    .balign 0x80
l64fiq3: b l64fiq3

    .balign 0x80
l64serr3: b l64serr3

//
// Lower EL using AArch32
//
    .balign 0x80
l32sync3: B l32sync3

    .balign 0x80
l32irq3: b l32irq3

    .balign 0x80
l32fiq3: b l32fiq3

    .balign 0x80
l32serr3: b l32serr3
