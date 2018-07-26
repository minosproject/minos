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

	.global get_cpu_id
	.global arch_get_fp
	.global arch_get_lr

func get_cpu_id
	mrs 	x0, MPIDR_EL1
	ubfx    x0, x0, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ret
endfunc get_cpu_id

func arch_get_fp
	mov	x0, x29
	ret
endfunc arch_get_fp

func arch_get_lr
	mov	x0, x30
	ret
endfunc arch_get_lr
