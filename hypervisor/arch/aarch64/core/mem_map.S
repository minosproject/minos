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

#include <asm/asm_mmu.h>
#include <asm/aarch64_common.h>
#include <config/config.h>

	/*
	 * map the code memory VA->PA, if need to using
	 * dcache need to enable the MMU, first clear
	 * the page table, below is the var defination
	 * Note : Do not use x19, since it has been used
	 * to save the cpuid
	 */
ttb0_pgd	.req	x20
ttb0_pud	.req	x21
ttb0_pmd	.req	x22

vaddr		.req	x0
paddr		.req	x1
size		.req	x2
pte_attr	.req	x3

pud_tmp		.req	x10
pmd_tmp		.req	x12
tmp		.req	x13
pte_value	.req	x14
pte_index	.req	x15
entry_size	.req	x16
entry_align	.req	x17
entry_mask	.req	x18

	.section __start_up, "ax"
	.balign 4

	.global	map_boot_mem

map_boot_mem:
	/* save the lr register */
	mov	x28, x30

	mov	x1, #0
	mov	x2, #4096
	ldr	ttb0_pgd, = __el2_ttb0_pgd
	mov	x0, x20
	bl	memset
	ldr	ttb0_pud, = __el2_ttb0_pud
	mov	x0, x21
	mov	x1, #0
	mov	x2, #4096
	bl	memset
	ldr	ttb0_pmd, = __el2_ttb0_pmd
	mov	x0, ttb0_pmd
	mov	x1, #0
	mov	x2, #0x4000
	bl	memset

	bl	build_pgd_table
	dsb	sy

	bl	build_pud_table
	dsb	sy

	ldr	vaddr, =CONFIG_MINOS_START_ADDRESS
	ldr	paddr, =CONFIG_MINOS_START_ADDRESS
	ldr	size, =CONFIG_MINOS_RAM_SIZE
	mov	pte_attr, #(TT_S1_ATTR_BLOCK | \
		(4 << TT_S1_ATTR_MATTR_LSB) | \
		TT_S1_ATTR_AP_RW_PL1 | \
		TT_S1_ATTR_NS | \
		TT_S1_ATTR_SH_INNER | \
		TT_S1_ATTR_AF)
	bl	build_pmd_table
	dsb	sy

	ldr	vaddr, =CONFIG_UART_BASE
	ldr	paddr, =CONFIG_UART_BASE
	ldr	size, =CONFIG_UART_IO_SIZE
	ldr	pte_attr, =(TT_S1_ATTR_BLOCK | \
		(0 << TT_S1_ATTR_MATTR_LSB) | \
		TT_S1_ATTR_AP_RW_PL1 | \
		TT_S1_ATTR_NS | \
		TT_S1_ATTR_UXN | \
		TT_S1_ATTR_PXN | \
		TT_S1_ATTR_AF)
	bl	build_pmd_table
	dsb	sy
	ret	x28

build_pmd_table:
	/* map vaddr must smaller than 4G */
	ldr	entry_size, =0x200000
	ldr	entry_mask, =0x1fffff
	ldr	entry_align, =0xffffffffffe00000

	add	tmp, vaddr, size
	add	tmp, tmp, entry_mask
	and	vaddr, vaddr, entry_align
	and	paddr, paddr, entry_align
	and	tmp, tmp, entry_align
	sub	size, tmp, vaddr		//size vaddr paddr align with 2M

	mov	pmd_tmp, ttb0_pmd
	ubfx	pte_index, vaddr, #21, #11
	add	pmd_tmp, pmd_tmp, pte_index, lsl #3

	bic	paddr, paddr, entry_mask
	bic	paddr, paddr, #0xffff000000000000
	orr	paddr, paddr, pte_attr

loop_pmd:
	cbz	size, exit_loop
	str	paddr, [pmd_tmp]
	sub	size, size, entry_size
	add	pmd_tmp, pmd_tmp, #8
	add	paddr, paddr, entry_size
	b	loop_pmd
exit_loop:
	ret

build_pud_table:
	/* map first 4GB PUD for booting memory */
	ldr	vaddr, =0x0
	ldr	paddr, =0x0
	ldr	size, =0x100000000

	ldr	entry_size, =0x40000000
	mov	tmp, ttb0_pmd
	mov	pte_index, 0

loop_pud:
	orr	pte_value, tmp, #TT_S1_ATTR_TABLE
	str	pte_value, [ttb0_pud, pte_index, lsl #3]
	add	pte_index, pte_index, #1
	add	tmp, tmp, #0x1000
	sub	size, size, entry_size
	cbnz	size, loop_pud
	ret

build_pgd_table:
	/* map first 512GB for booting memory */
	orr	pte_value, ttb0_pud, #TT_S1_ATTR_TABLE
	str	pte_value, [ttb0_pgd]
	ret
