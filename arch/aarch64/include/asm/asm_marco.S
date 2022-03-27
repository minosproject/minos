#ifndef _MINOS_ASM_MARCO_H_
#define _MINOS_ASM_MARCO_H_

#include <config/config.h>

// current kernel and user space all used 512G virtual
// address range, so:
// ttbr0_el1 : 0x0000000000000000 - 0x00000007fffffffff
// ttbr1_el1 : 0xfffffff800000000 - 0xfffffffffffffffff

#ifdef CONFIG_ARM_ADDRESS_TAGGING
.macro asm_vtop reg
	and	\reg, \reg, #CONFIG_VTOP_MASK
.endm

.macro asm_ptov reg
	orr	\reg, \reg, #CONFIG_PTOV_MASK
.endm
#else
.macro asm_vtop reg
.endm

.macro asm_ptov reg
.endm
#endif

.macro func _name, align=2
	.cfi_sections .debug_frame
	.section __asm_code, "ax"
	.type \_name, %function
	.func \_name
	.cfi_startproc
	.align \align
	\_name:
.endm

.macro endfunc _name
	.endfunc
	.cfi_endproc
	.size \_name, . - \_name
.endm

.macro PRINT
	adr	x0, 98f
	bl	puts
	b	99f
98:     .asciz "-------1\r\n"
	.align 2
99:
.endm

#endif
