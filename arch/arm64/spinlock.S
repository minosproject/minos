/*
 * Created by Le Min 2017/12/12
 */

#include "include/asm_marco.S"

	.global arch_spin_lock
	.global arch_spin_unlock

func arch_spin_lock
	mov	w2, #1
	sevl
l1:	wfe
l2:	ldaxr	w1, [x0]
	cbnz	w1, l1
	stxr	w1, w2, [x0]
	cbnz	w1, l1
	ret
endfunc arch_spin_lock

func arch_spin_unlock
	stlr	wzr, [x0]
	ret
endfunc arch_spin_unlock
