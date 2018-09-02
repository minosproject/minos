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

#define LOG_BUFFER_SIZE		(8192)

struct log_buffer {
	spinlock_t buffer_lock;
	int tail;
	int total;
	char buf[LOG_BUFFER_SIZE];
};

static struct log_buffer log_buffer;

void log_init(void)
{
	spin_lock_init(&log_buffer.buffer_lock);
	log_buffer.tail = 0;
	log_buffer.total = 0;
}

static int update_log_buffer(char *buf, int printed)
{
	int len;
	struct log_buffer *lb = &log_buffer;

	while (printed) {
		if ((lb->tail + printed) > (LOG_BUFFER_SIZE - 1)) {
			len = LOG_BUFFER_SIZE - lb->tail;
			memcpy(lb->buf + lb->tail, buf, len);
			lb->tail = 0;
		} else {
			len = printed > LOG_BUFFER_SIZE ? LOG_BUFFER_SIZE : printed;
			memcpy(lb->buf + lb->tail, buf, len);
			lb->tail += len;
		}

		printed -= len;
		buf += len;
		lb->total += printed;
	}

	return 0;
}

static char buffer[1024];

int level_print(char *fmt, ...)
{
	char ch;
	va_list arg;
	int printed, i;
	char *buf;

	ch = fmt[2];
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

	spin_lock(&log_buffer.buffer_lock);

	va_start(arg, fmt);
	printed = vsprintf(buffer, fmt, arg);
	va_end(arg);

	/*
	 * temp disable the log buffer
	 */
	update_log_buffer(buffer, printed);
	buf = buffer;

	for(i = 0; i < printed; i++)
		serial_putc(*buf++);

	spin_unlock(&log_buffer.buffer_lock);

	return printed;
}
