#include "include/asm_marco.S"

	.align 5

	.global switch_to_vcpu

func switch_to_vcpu
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
	isb
	eret
endfunc switch_to_vcpu
