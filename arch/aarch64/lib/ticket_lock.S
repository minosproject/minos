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

#include <asm/asm_marco.S>

	.global arch_ticket_lock
	.global arch_ticket_unlock

func arch_ticket_lock
	prfm    pstl1keep, [x0]
	add	x4, x0, #4
loop1:
	ldaxr	w2, [x4]
	add	w2, w2, #1
	stlxr	w3, w2, [x4]
	cbnz	w3, loop1
	sub	w2, w2, #1

	ldaxr	w1, [x0]
	cmp	w1, w2
	b.eq	out

	sevl
loop2:
	wfe
	ldaxr	w1, [x0]
	cmp	w1, w2
	b.ne	loop2
out:
	ret
endfunc arch_ticket_lock

func arch_ticket_unlock
	ldar	w1, [x0]
	add	w1, w1, #1
	stlr	w1, [x0]
	ret
endfunc arch_ticket_unlock
