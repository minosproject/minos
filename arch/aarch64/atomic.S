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

	.global atomic_add
	.global atomic_sub
	.global atomic_add_return
	.global atomic_sub_return

	/*
	 * ldaxr : a - acquire (equal add a dmb ins)
	 *         x - exclusive
	 * ldar ldarb ldarh ldxr ldxrh
	 *
	 * stlxr : l - release (equal add a dmb ins)
	 *       : x - exclusive
	 * stlr stxr stlrb stlrh stxrh etc
	 */

func atomic_add
1:
	ldxr	w2, [x1]
	add	w2, w2, w0
	stxr	w3, w2, [x1]
	cbnz	w3, 1b
	ret
endfunc atomic_add

func atomic_sub
2:
	ldxr	w2, [x1]
	sub	w2, w2, w0
	stxr	w3, w2, [x1]
	cbnz	w3, 2b
	ret
endfunc atomic_sub

func atomic_add_return
3:
	ldxr	w2, [x1]
	add	w2, w2, w0
	stlxr	w3, w2, [x1]
	cbnz	w3, 3b
	dmb	ish
	mov	w0, w2
	ret
endfunc atomic_add_return

func atomic_sub_return
4:
	ldxr	w2, [x1]
	sub	w2, w2, w0
	stxr	w3, w2, [x1]
	cbnz	w3, 4b
	dmb	ish
	mov	w0, w2
	ret
endfunc atomic_sub_return
