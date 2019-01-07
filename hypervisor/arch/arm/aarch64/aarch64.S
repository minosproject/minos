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
#include <config/config.h>

	.global smp_processor_id
	.global arch_get_fp
	.global arch_get_lr
	.global arch_get_sp
	.global smc_call

func smp_processor_id
	mrs 	x0, MPIDR_EL1
#ifdef CONFIG_MPIDR_SHIFT
	ubfx    x1, x0, #MPIDR_EL1_AFF1_LSB, #MPIDR_EL1_AFF_WIDTH
	ubfx    x2, x0, #MPIDR_EL1_AFF2_LSB, #MPIDR_EL1_AFF_WIDTH
#else
	ubfx    x1, x0, #MPIDR_EL1_AFF0_LSB, #MPIDR_EL1_AFF_WIDTH
	ubfx    x2, x0, #MPIDR_EL1_AFF1_LSB, #MPIDR_EL1_AFF_WIDTH
#endif
	mov	x3, #CONFIG_NR_CPUS_CLUSTER0
	mul	x2, x2, x3
	add	x0, x2, x1
	ret
endfunc smp_processor_id

func arch_get_fp
	mov	x0, x29
	ret
endfunc arch_get_fp

func arch_get_lr
	mov	x0, x30
	ret
endfunc arch_get_lr

func arch_get_sp
	mov	x0, sp
	ret
endfunc arch_get_sp

func smc_call
	smc	#0
	ret
endfunc smc_call
