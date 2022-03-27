/*
 * Copyright (C) 2020 Min Le (lemin9538@gmail.com)
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

#ifndef __MINOS_AARCH64_REG_H__
#define __MINOS_AARCH64_REG_H__

#include <config/config.h>

#ifdef CONFIG_VIRT
	#include <asm/aarch64_el2_reg.h>
#else
	#include <asm/aarch64_el1_reg.h>
#endif	// CONFIG_VIRT

#include <asm/gic_reg.h>

#endif
