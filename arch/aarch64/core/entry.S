/*
 * Copyright (C) 2004-2017 ARM Ltd. All rights reserved.
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
#include <config/config.h>
#include <asm/asm_marco.S>

// .align n = align 2^n
// balign 0x80 = align with 0x80 boundary.

	.global elx_vectors

	.section __el2_vectors, "ax"
	.align 12

.macro BAD_MODE mode
	stp	x29, x30, [sp, #-16]
	mov	x29, #\mode
	b	__bad_mode
.endm

elx_vectors:
c0sync:		// Current EL with SP0
	BAD_MODE VECTOR_C0_SYNC
	.balign 0x80
c0irq:
	BAD_MODE VECTOR_C0_IRQ
	.balign 0x80
c0fiq:
	BAD_MODE VECTOR_C0_FIQ
	.balign 0x80
c0serr:
	BAD_MODE VECTOR_C0_SERR
	.balign 0x80	// Current EL with SPx
cxsync:
	b __sync_exception_from_current_el
	.balign 0x80
cxirq:
	b __irq_exception_from_current_el
	.balign 0x80
cxfiq:
	BAD_MODE VECTOR_CX_FIQ
	.balign 0x80
cxserr:
	BAD_MODE VECTOR_CX_SERR
	.balign 0x80	//Lower EL using AArch64
l64sync:
	b __sync_exception_from_lower_el
	.balign 0x80
l64irq:
	b __irq_exception_from_lower_el
	.balign 0x80
l64fiq:
	BAD_MODE VECTOR_L64_FIQ
	.balign 0x80
l64serr:
	BAD_MODE VECTOR_L64_SERR
	.balign 0x80	// Lower EL using AArch32
l32sync:
	b __sync_exception_from_lower_el
	.balign 0x80
l32irq:
	b __irq_exception_from_lower_el
	.balign 0x80
l32fiq:
	BAD_MODE VECTOR_L32_FIQ
	.balign 0x80
l32serr:
	BAD_MODE VECTOR_L32_SERR
	.balign 0x80
