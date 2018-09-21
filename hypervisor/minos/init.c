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

#include <minos/init.h>
#include <minos/types.h>
#include <minos/print.h>
#include <minos/arch.h>
#include <minos/platform.h>
#include <minos/string.h>
#include <minos/calltrace.h>

extern unsigned char __init_func_0_start;
extern unsigned char __init_func_1_start;
extern unsigned char __init_func_2_start;
extern unsigned char __init_func_3_start;
extern unsigned char __init_func_4_start;
extern unsigned char __init_func_5_start;
extern unsigned char __init_func_6_start;
extern unsigned char __init_func_7_start;
extern unsigned char __init_func_8_start;
extern unsigned char __init_func_9_start;
extern unsigned char __init_func_end;
extern void log_init(void);

static void call_init_func(unsigned long fn_start, unsigned long fn_end)
{
	init_call *fn;
	int size, i;

	size = (fn_end - fn_start) / sizeof(init_call);
	pr_debug("call init func : 0x%x 0x%x %d\n", fn_start, fn_end, size);

	if (size <= 0)
		return;

	fn = (init_call *)fn_start;
	for (i = 0; i < size; i++) {
		(*fn)();
		fn++;
	}
}

void platform_early_init(void)
{
	int i, count;
	extern unsigned char __platform_start;
	extern unsigned char __platform_end;
	unsigned long pstart;
	unsigned long pend;
	struct platform **pp;
	struct platform *p;

	log_init();

	pstart =(unsigned long)&__platform_start;
	pend = (unsigned long)&__platform_end;
	count = (pend - pstart) / sizeof(struct platform *);
	pp = (struct platform **)pstart;

	if (count == 0)
		panic("no platform found for minos");

	for (i = 0; i < count; i++) {
		p = *pp;
		if (strcmp(p->name, CONFIG_PLATFORM_NAME) == 0) {
			platform = p;
			break;
		}

		pp++;
	}

	if (platform == NULL)
		panic("no platform found for minos\n");

	pr_info("platform-%s\n", platform->name);
}

void early_init(void *setup_data)
{
	/* get the platform for the minos */
	arch_early_init(setup_data);

	call_init_func((unsigned long)&__init_func_0_start,
			(unsigned long)&__init_func_1_start);
}

void arch_init(void)
{
	__arch_init();

	call_init_func((unsigned long)&__init_func_1_start,
			(unsigned long)&__init_func_2_start);
}

void subsys_init(void)
{
	call_init_func((unsigned long)&__init_func_2_start,
			(unsigned long)&__init_func_3_start);
}

void module_init(void)
{
	call_init_func((unsigned long)&__init_func_3_start,
			(unsigned long)&__init_func_4_start);
}

void device_init(void)
{
	call_init_func((unsigned long)&__init_func_4_start,
			(unsigned long)&__init_func_5_start);
}

void early_init_percpu(void)
{
	call_init_func((unsigned long)&__init_func_5_start,
			(unsigned long)&__init_func_6_start);
}

void arch_init_percpu(void)
{
	call_init_func((unsigned long)&__init_func_6_start,
			(unsigned long)&__init_func_7_start);
}

void subsys_init_percpu(void)
{
	call_init_func((unsigned long)&__init_func_7_start,
			(unsigned long)&__init_func_8_start);
}

void module_init_percpu(void)
{
	call_init_func((unsigned long)&__init_func_8_start,
			(unsigned long)&__init_func_9_start);
}

void device_init_percpu(void)
{
	call_init_func((unsigned long)&__init_func_9_start,
			(unsigned long)&__init_func_end);
}
