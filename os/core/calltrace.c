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
#include <minos/smp.h>

static unsigned long *allsyms_address;
static unsigned int *allsyms_offset;
static int allsyms_count;
static char *allsyms_names;

extern unsigned char __symbols_start;

static DEFINE_SPIN_LOCK(dump_lock);

void dump_stack(gp_regs *regs, unsigned long *stack)
{
	unsigned long flags;

	spin_lock_irqsave(&dump_lock, flags);
	arch_dump_stack(regs, stack);
	spin_unlock_irqrestore(&dump_lock, flags);
}

static void panic_other_cpu(void *data)
{
	pr_fatal("[Panic called by other cpu]");
	dump_stack(NULL, NULL);

	for (;;)
		cpu_relax();
}

void __panic(gp_regs *regs, char *fmt, ...)
{
	int cpu;
	va_list arg;
	int printed;
	char buffer[512];

	va_start(arg, fmt);
	printed = vsprintf(buffer, fmt, arg);
	va_end(arg);

	printed = printed >= 512 ? 511 : printed;
	buffer[printed + 1] = 0;

	pr_fatal("[Panic] : %s", buffer);
	dump_stack(regs, NULL);

	/* inform other cpu to do panic */
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (cpu == smp_processor_id())
			continue;

		smp_function_call(cpu, panic_other_cpu, NULL, 0);
	}

	for (;;)
		cpu_relax();
}

static int locate_symbol_pos(unsigned long addr)
{
	int left = 0, right = 1;

	if (allsyms_count <= 0)
		return -1;

	while (1) {
		if (right == allsyms_count)
			break;;

		if (addr == allsyms_address[right])
			return right;

		if ((addr >= allsyms_address[left]) &&
				(addr < allsyms_address[right]))
			return left;

		left++;
		right++;
	}

	if ((addr >= allsyms_address[left]) &&
		(addr < (unsigned long)&__symbols_start))
		return left;

	return -1;
}

void print_symbol(unsigned long addr)
{
	int pos;
	unsigned long symbol_left;
	unsigned long symbol_right;
	unsigned int offset;
	char *name = NULL;

	pos = locate_symbol_pos(addr);
	if (pos == -1)
		return;

	symbol_left = allsyms_address[pos];
	if (pos == (allsyms_count - 1))
		symbol_right = (unsigned long)&__symbols_start;
	else
		symbol_right = allsyms_address[pos + 1];

	offset = allsyms_offset[pos];
	name = allsyms_names + offset;

	pr_error("[%p] ? %s+0x%x/0x%x\n", addr, name,
			addr - symbol_left,
			symbol_right - symbol_left);
}

int allsymbols_init(void)
{
	int *tmp;
	unsigned long *symbols_base;

	symbols_base = (unsigned long *)&__symbols_start;
	allsyms_address = (unsigned long *)(*symbols_base++);
	tmp = (int *)(*symbols_base++);
	allsyms_count = *tmp;
	allsyms_offset = (unsigned int *)(*symbols_base++);
	allsyms_names = (char *)(*symbols_base);

	return 0;
}
