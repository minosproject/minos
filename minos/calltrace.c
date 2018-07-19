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

#include <minos/minos.h>
#include <asm/arch.h>

static DEFINE_SPIN_LOCK(dump_lock);

void dump_stack(gp_regs *regs, unsigned long *stack)
{
	unsigned long flags;

	spin_lock_irqsave(&dump_lock, flags);
	arch_dump_stack(regs, stack);
	spin_unlock_irqrestore(&dump_lock, flags);
}

void panic(char *str)
{
	pr_fatal("[Panic] : %s", str);
	dump_stack(NULL, NULL);
	while (1);
}
