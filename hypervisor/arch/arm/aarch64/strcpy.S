/*
 * Copyright (C) 2013 ARM Ltd.
 * Copyright (C) 2013 Linaro.
 * Copyright (C) 2014 The Android Open Source Project
 *
 * This code is based on glibc cortex strings work originally authored by Linaro
 * and re-licensed under GPLv2 for the Linux kernel. The original code can
 * be found @
 *
 * http://bazaar.launchpad.net/~linaro-toolchain-dev/cortex-strings/trunk/
 * files/head:/src/aarch64/
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

	.global strcpy

dst_in		.req x0
src		.req x1
dst		.req x2
data_1		.req x3
data_1_w	.req w3
data2		.req x4
data2_w		.req w4
has_nul1	.req x5
has_nul1_w	.req w5
has_nul2	.req x6
tmp1		.req x7
tmp2		.req x8
tmp3		.req x9
tmp4		.req x10
zero_oness	.req x11
zero_oness_w	.req w11
pos		.req x12

#define REP8_01 0x0101010101010101
#define REP8_7f 0x7f7f7f7f7f7f7f7f
#define REP8_80 0x8080808080808080

func strcpy
    mov     zero_oness, #REP8_01
    mov     dst, dst_in
    ands    tmp1, src, #15
    b.ne    .Lmisaligned
.Lloop:
    ldp     data_1, data2, [src], #16
    sub     tmp1, data_1, zero_oness
    orr     tmp2, data_1, #REP8_7f
    bic     has_nul1, tmp1, tmp2
    cbnz    has_nul1, .Lnul_in_data_1
    sub     tmp3, data2, zero_oness
    orr     tmp4, data2, #REP8_7f
    bic     has_nul2, tmp3, tmp4
    cbnz    has_nul2, .Lnul_in_data2
    stp     data_1, data2, [dst], #16
    b       .Lloop

.Lnul_in_data_1:
    rev     has_nul1, has_nul1
    clz     pos, has_nul1
    add     tmp1, pos, #0x8

    tbz     tmp1, #6, 1f
    str     data_1, [dst]
    ret
1:
    tbz     tmp1, #5, 1f
    str     data_1_w, [dst], #4
    lsr     data_1, data_1, #32
1:
    tbz     tmp1, #4, 1f
    strh    data_1_w, [dst], #2
    lsr     data_1, data_1, #16
1:
    tbz     tmp1, #3, 1f
    strb    data_1_w, [dst]
1:
    ret

.Lnul_in_data2:
    str     data_1, [dst], #8
    rev     has_nul2, has_nul2
    clz     pos, has_nul2
    add     tmp1, pos, #0x8

    tbz     tmp1, #6, 1f
    str     data2, [dst]
    ret
1:
    tbz     tmp1, #5, 1f
    str     data2_w, [dst], #4
    lsr     data2, data2, #32
1:
    tbz     tmp1, #4, 1f
    strh    data2_w, [dst], #2
    lsr     data2, data2, #16
1:
    tbz     tmp1, #3, 1f
    strb    data2_w, [dst]
1:
    ret

.Lmisaligned:
    tbz     src, #0, 1f
    ldrb    data_1_w, [src], #1
    strb    data_1_w, [dst], #1
    cbnz    data_1_w, 1f
    ret
1:
    tbz     src, #1, 1f
    ldrb    data_1_w, [src], #1
    strb    data_1_w, [dst], #1
    cbz     data_1_w, .Ldone
    ldrb    data2_w, [src], #1
    strb    data2_w, [dst], #1
    cbnz    data2_w, 1f
.Ldone:
    ret
1:
    tbz     src, #2, 1f
    ldr     data_1_w, [src], #4
    sub     has_nul1_w, data_1_w, zero_oness_w
    bic     has_nul1_w, has_nul1_w, data_1_w
    ands    has_nul1_w, has_nul1_w, #0x80808080
    b.ne    .Lnul_in_data_1
    str     data_1_w, [dst], #4
1:
    tbz     src, #3, .Lloop
    ldr     data_1, [src], #8
    sub     tmp1, data_1, zero_oness
    orr     tmp2, data_1, #REP8_7f
    bics    has_nul1, tmp1, tmp2
    b.ne    .Lnul_in_data_1
    str     data_1, [dst], #8
    b       .Lloop
endfunc strcpy
