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

#include <linux/linkage.h>
#include <asm/assembler.h>

ENTRY(__minos_hvc)
	hvc 	0
	mov	x14, x1
	mov	x15, x2
	mov	x16, x3
	ret
ENDPROC(__minos_hvc)

ENTRY(minos_hvc_result1)
	str	x14, [x0]
	ret
ENDPROC(minos_hvc_result1)

ENTRY(minos_hvc_result2)
	str	x14, [x0]
	str	x15, [x1]
	ret
ENDPROC(minos_hvc_result2)

ENTRY(minos_hvc_result3)
	str	x14, [x0]
	str	x15, [x1]
	str	x16, [x2]
	ret
ENDPROC(minos_hvc_result3)

