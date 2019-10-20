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

#include <minos/types.h>
#include <minos/varlist.h>
#include <minos/string.h>
#include <minos/print.h>
#include <minos/preempt.h>
#include <config/config.h>
#include <drivers/serial.h>
#include <minos/smp.h>
#include <minos/time.h>
#include <minos/task.h>
#include <minos/sched.h>

#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL	PRINT_LEVEL_INFO
#endif

extern struct task *__current_tasks[NR_CPUS];

static DEFINE_SPIN_LOCK(print_lock);
static unsigned int print_level = CONFIG_LOG_LEVEL;

static int get_print_time(char *buffer)
{
	unsigned long us;
	unsigned long second;
	int len, left;
	char buf[64];

	us = get_current_time() / 1000;
	second = us / 1000000;
	us = us % 1000000;

	memset(buf, '0', 64);
	len = uitoa(buf, second);
	len = len > 8 ? 8 : len;
	left = 8 - len;

	if (left > 0) {
		memset(buffer, ' ', left);
		buffer += left;
	}
	memcpy(buffer, buf, len);
	buffer += len;

	*buffer++ = '.';

	memset(buf, '0', 8);
	len = uitoa(buf, us);
	len = len > 6 ? 6 : len;
	left = 6 - len;

	if (left > 0) {
		memset(buffer, '0', left);
		buffer += left;
	}
	memcpy(buffer, buf, len);

	return 15;
}

void change_log_level(unsigned int level)
{
	print_level = level;
}

int level_print(int level, char *fmt, ...)
{
	va_list arg;
	int printed, i, cpuid;
	char buf[512];
	char *buffer = buf;
	unsigned long flags;
	int pid;
	struct task *task;

	if (level > print_level)
		return 0;

	cpuid = smp_processor_id();
	task = __current_tasks[cpuid];
	if (task && os_is_running())
		pid = get_task_pid(task);
	else
		pid = 999;

	/*
	 * after to handle the level we change
	 * the level to the current CPU
	 */
	*buffer++ = '[';

	/* get the time of the log when it print */
	buffer += get_print_time(buffer);
	*buffer++ = ' ';

	/* add the processor id to the buffer */
	*buffer++ = (cpuid / 10) + '0';
	*buffer++ = (cpuid % 10) + '0';

	/* add the task pid to the buffer */
	*buffer++ = ' ';
	*buffer++ = (pid / 100) + '0';
	*buffer++ = ((pid % 100) / 10) + '0';
	*buffer++ = (pid % 10) + '0';

	*buffer++ = ']';
	*buffer++ = ' ';

	/*
	 * TBD need to check the length of fmt
	 * in case of buffer overflow
	 */
	printed = buffer - buf;
	va_start(arg, fmt);
	printed += vsprintf(buffer, fmt, arg);
	va_end(arg);

	spin_lock_irqsave(&print_lock, flags);
	for(i = 0; i < printed; i++)
		serial_putc(buf[i]);
	spin_unlock_irqrestore(&print_lock, flags);

	return printed;
}
