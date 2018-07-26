/*
 * Copyright (C) 2001 Deep Blue Solutions Ltd.
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (C) 1996-2000 Russell King
 * Copyright (C) 2012 ARM Ltd.
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

	.global flush_dcache_range
	.global clean_dcache_range
	.global inv_dcache_range
	.global dcsw_op_louis
	.global dcsw_op_all
	.global dcsw_op_level1
	.global dcsw_op_level2
	.global dcsw_op_level3
	.global inv_unified_dcache

.macro	dcache_line_size  reg, tmp
	mrs	\tmp, ctr_el0
	ubfx	\tmp, \tmp, #16, #4
	mov	\reg, #4
	lsl	\reg, \reg, \tmp
.endm

.macro	icache_line_size  reg, tmp
	mrs	\tmp, ctr_el0
	and	\tmp, \tmp, #0xf
	mov	\reg, #4
	lsl	\reg, \reg, \tmp
.endm

.macro do_dcache_maintenance_by_mva op
	/* Exit early if size is zero */
	cbz	x1, exit_loop_\op
	dcache_line_size x2, x3
	add	x1, x0, x1
	sub	x3, x2, #1
	bic	x0, x0, x3
loop_\op:
	dc	\op, x0
	add	x0, x0, x2
	cmp	x0, x1
	b.lo    loop_\op
	dsb	sy
exit_loop_\op:
	ret
.endm


func flush_dcache_range
	do_dcache_maintenance_by_mva civac
endfunc flush_dcache_range

func inv_dcache_range
	do_dcache_maintenance_by_mva ivac
endfunc inv_dcache_range

.macro	dcsw_op shift, fw, ls
	mrs	x9, clidr_el1
	ubfx	x3, x9, \shift, \fw
	lsl	x3, x3, \ls
	mov	x10, xzr
	b	do_dcsw_op
.endm

func do_dcsw_op
	cbz	x3, exit
	adr	x14, dcsw_loop_table	// compute inner loop address
	add	x14, x14, x0, lsl #5	// inner loop is 8x32-bit instructions
	mov	x0, x9
	mov	w8, #1
loop1:
	add	x2, x10, x10, lsr #1	// work out 3x current cache level
	lsr	x1, x0, x2		// extract cache type bits from clidr
	and	x1, x1, #7		// mask the bits for current cache only
	cmp	x1, #2			// see what cache we have at this level
	b.lo	level_done		// nothing to do if no cache or icache

	msr	csselr_el1, x10		// select current cache level in csselr
	isb				// isb to sych the new cssr&csidr
	mrs	x1, ccsidr_el1		// read the new ccsidr
	and	x2, x1, #7		// extract the length of the cache lines
	add	x2, x2, #4		// add 4 (line length offset)
	ubfx	x4, x1, #3, #10		// maximum way number
	clz	w5, w4			// bit position of way size increment
	lsl	w9, w4, w5		// w9 = aligned max way number
	lsl	w16, w8, w5		// w16 = way number loop decrement
	orr	w9, w10, w9		// w9 = combine way and cache number
	ubfx	w6, w1, #13, #15	// w6 = max set number
	lsl	w17, w8, w2		// w17 = set number loop decrement
	dsb	sy			// barrier before we start this level
	br	x14			// jump to DC operation specific loop

	.macro	dcsw_loop _op
loop2_\_op:
	lsl	w7, w6, w2		// w7 = aligned max set number

loop3_\_op:
	orr	w11, w9, w7		// combine cache, way and set number
	dc	\_op, x11
	subs	w7, w7, w17		// decrement set number
	b.hs	loop3_\_op

	subs	x9, x9, x16		// decrement way number
	b.hs	loop2_\_op

	b	level_done
	.endm

level_done:
	add	x10, x10, #2		// increment cache number
	cmp	x3, x10
	b.hi    loop1
	msr	csselr_el1, xzr		// select cache level 0 in csselr
	dsb	sy			// barrier to complete final cache operation
	isb
exit:
	ret
endfunc do_dcsw_op

dcsw_loop_table:
	dcsw_loop isw
	dcsw_loop cisw
	dcsw_loop csw

func dcsw_op_louis
	dcsw_op #LOUIS_SHIFT, #CLIDR_FIELD_WIDTH, #LEVEL_SHIFT
endfunc dcsw_op_louis


func dcsw_op_all
	dcsw_op #LOC_SHIFT, #CLIDR_FIELD_WIDTH, #LEVEL_SHIFT
endfunc dcsw_op_all

	/* ---------------------------------------------------------------
	 *  Helper macro for data cache operations by set/way for the
	 *  level specified
	 * ---------------------------------------------------------------
	 */
	.macro dcsw_op_level level
	mrs	x9, clidr_el1
	mov	x3, \level
	sub	x10, x3, #2
	b	do_dcsw_op
	.endm

	/* ---------------------------------------------------------------
	 * Data cache operations by set/way for level 1 cache
	 *
	 * The main function, do_dcsw_op requires:
	 * x0: The operation type (0-2), as defined in arch.h
	 * ---------------------------------------------------------------
	 */
func dcsw_op_level1
	dcsw_op_level #(1 << LEVEL_SHIFT)
endfunc dcsw_op_level1

	/* ---------------------------------------------------------------
	 * Data cache operations by set/way for level 2 cache
	 *
	 * The main function, do_dcsw_op requires:
	 * x0: The operation type (0-2), as defined in arch.h
	 * ---------------------------------------------------------------
	 */
func dcsw_op_level2
	dcsw_op_level #(2 << LEVEL_SHIFT)
endfunc dcsw_op_level2

	/* ---------------------------------------------------------------
	 * Data cache operations by set/way for level 3 cache
	 *
	 * The main function, do_dcsw_op requires:
	 * x0: The operation type (0-2), as defined in arch.h
	 * ---------------------------------------------------------------
	 */
func dcsw_op_level3
	dcsw_op_level #(3 << LEVEL_SHIFT)
endfunc dcsw_op_level3

func inv_unified_dcache
	// From the ARM ARMv8-A Architecture Reference Manual
	dmb	ish                   // ensure all prior inner-shareable accesses have been observed
	mrs	x0, CLIDR_EL1
	and	w3, w0, #0x07000000   // get 2 x level of coherence
	lsr	w3, w3, #23
	cbz	w3, finished
	mov	w10, #0               // w10 = 2 x cache level
	mov	w8, #1                // w8 = constant 0b1
loop_level:
	add	w2, w10, w10, lsr #1  // calculate 3 x cache level
	lsr	w1, w0, w2            // extract 3-bit cache type for this level
	and	w1, w1, #0x7
	cmp	w1, #2
	b.lt	next_level            // no data or unified cache at this level
	msr	CSSELR_EL1, x10       // select this cache level
	isb			      // synchronize change of csselr
	mrs	x1, CCSIDR_EL1        // read ccsidr
	and	w2, w1, #7            // w2 = log2(linelen)-4
	add	w2, w2, #4            // w2 = log2(linelen)
	ubfx	w4, w1, #3, #10       // w4 = max way number, right aligned
	clz	w5, w4                // w5 = 32-log2(ways), bit position of way in dc operand
	lsl	w9, w4, w5            // w9 = max way number, aligned to position in dc operand
	lsl	w16, w8, w5           // w16 = amount to decrement way number per iteration
loop_way:
	ubfx	w7, w1, #13, #15      // w7 = max set number, right aligned
	lsl	w7, w7, w2            // w7 = max set number, aligned to position in dc operand
	lsl	w17, w8, w2           // w17 = amount to decrement set number per iteration
loop_set:
	orr	w11, w10, w9          // w11 = combine way number and cache number ...
	orr	w11, w11, w7          // ... and set number for dc operand
	dc	isw, x11              // do data cache invalidate by set and way
	subs	w7, w7, w17           // decrement set number
	b.ge	loop_set
	subs	x9, x9, x16           // decrement way number
	b.ge	loop_way
next_level:
	add	w10, w10, #2          // increment 2 x cache level
	cmp	w3, w10
	b.gt	loop_level
	dsb	sy                    // ensure completion of previous cache maintenance operation
	isb
finished:
	ret
endfunc inv_unified_dcache
