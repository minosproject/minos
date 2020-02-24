/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asm/aarch64_common.h>
#include <asm/asm_marco.S>

	.global __atomic_set
	.global __atomic_get
	.global atomic_add
	.global atomic_sub
	.global atomic_add_return
	.global atomic_add_return_old
	.global atomic_sub_return
	.global atomic_sub_return_old

	/*
	 * ldaxr : a - acquire (equal add a dmb ins)
	 *         x - exclusive
	 * ldar ldarb ldarh ldxr ldxrh
	 *
	 * stlxr : l - release (equal add a dmb ins)
	 *       : x - exclusive
	 * stlr stxr stlrb stlrh stxrh etc
	 */

func __atomic_set
	stlr	w0, [x1]
	nop
	ret
endfunc __atomic_set

func __atomic_get
	ldr	w1, [x0]
	mov	w0, w1
	nop
	ret
endfunc __atomic_get

func atomic_add
1:
	ldaxr	w2, [x1]
	add	w2, w2, w0
	stxr	w3, w2, [x1]
	nop
	cbnz	w3, 1b
	ret
endfunc atomic_add

func atomic_sub
2:
	ldaxr	w2, [x1]
	sub	w2, w2, w0
	stxr	w3, w2, [x1]
	nop
	cbnz	w3, 2b
	ret
endfunc atomic_sub

func atomic_add_return
3:
	ldaxr	w2, [x1]
	add	w2, w2, w0
	stxr	w3, w2, [x1]
	nop
	cbnz	w3, 3b
	mov	w0, w2
	ret
endfunc atomic_add_return

func atomic_sub_return
4:
	ldaxr	w2, [x1]
	sub	w2, w2, w0
	stxr	w3, w2, [x1]
	nop
	cbnz	w3, 4b
	mov	w0, w2
	ret
endfunc atomic_sub_return

func atomic_add_return_old
3:
	ldaxr	w2, [x1]
	add	w2, w2, w0
	stxr	w3, w2, [x1]
	nop
	cbnz	w3, 3b
	sub	w0, w2, w0
	ret
endfunc atomic_add_return_old

func atomic_sub_return_old
4:
	ldaxr	w2, [x1]
	sub	w2, w2, w0
	stxr	w3, w2, [x1]
	nop
	cbnz	w3, 4b
	add	w0, w2, w0
	ret
endfunc atomic_sub_return_old
