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

	.global el2_vectors

	.section __el2_vectors, "ax"
	.align 11

el2_vectors:
c0sync2:		// Current EL with SP0
	b bad_mode
	.balign 0x80
c0irq2:
	b bad_mode
	.balign 0x80
c0fiq2:
	b bad_mode
	.balign 0x80
c0serr2:
	b bad_mode
	.balign 0x80	// Current EL with SPx
cxsync2:
	b __sync_handler
	.balign 0x80
cxirq2:
	b __irq_handler
	.balign 0x80
cxfiq2:
	b bad_mode
	.balign 0x80
cxserr2:
	bl bad_mode
	.balign 0x80	//Lower EL using AArch64
l64sync2:
	b __sync_handler
	.balign 0x80
l64irq2:
	b __irq_handler
	.balign 0x80
l64fiq2:
	b bad_mode
	.balign 0x80
l64serr2:
	b bad_mode
	.balign 0x80	// Lower EL using AArch32
l32sync2:
	b __sync_handler
	.balign 0x80
l32irq2:
	b __irq_handler
	.balign 0x80
l32fiq2:
	b bad_mode
	.balign 0x80
l32serr2:
	b bad_mode
