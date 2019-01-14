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
#include <minos/spinlock.h>
#include <config/config.h>
#include <drivers/serial.h>
#include <minos/smp.h>

static DEFINE_SPIN_LOCK(print_lock);

int level_print(char *fmt, ...)
{
	char ch;
	va_list arg;
	int printed, i;
	char buffer[1024];

	ch = fmt[4];
	if (is_digit(ch)) {
		ch = ch - '0';
		if(ch > CONFIG_LOG_LEVEL)
			return 0;
	}

	/*
	 * after to handle the level we change
	 * the level to the current CPU
	 */
	i = smp_processor_id();
	fmt[1] = (i / 10) + '0';
	fmt[2] = (i % 10) + '0';

	/*
	 * TBD need to check the length of fmt
	 * in case of buffer overflow
	 */
	va_start(arg, fmt);
	printed = vsprintf(buffer, fmt, arg);
	va_end(arg);

	spin_lock(&print_lock);
	for(i = 0; i < printed; i++)
		serial_putc(buffer[i]);
	spin_unlock(&print_lock);

	return printed;
}
