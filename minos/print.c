#include <minos/types.h>
#include <minos/varlist.h>
#include <minos/string.h>
#include <minos/print.h>
#include <minos/spinlock.h>
#include <config/config.h>
#include <drivers/pl011.h>
#include <minos/smp.h>

#define LOG_BUFFER_SIZE		(8192)

struct log_buffer {
	spinlock_t buffer_lock;
	int head;
	int tail;
	int total;
	char buf[LOG_BUFFER_SIZE];
};

static struct log_buffer log_buffer;
static int uart_init_done = 0;

void log_init(void)
{
	spin_lock_init(&log_buffer.buffer_lock);
	log_buffer.head = 0;
	log_buffer.tail = 0;
	log_buffer.total = 0;
}

void flush_log_buf(void)
{
	int i;

	if (uart_init_done)
		return;

	for (i = 0; i < log_buffer.tail - log_buffer.head; i++)
		uart_putc(log_buffer.buf[i]);

	uart_init_done = 1;
}

static int update_log_buffer(char *buf, int printed)
{
	struct log_buffer *lb = &log_buffer;
	int len1, len2;

	if ((lb->tail + printed) > (LOG_BUFFER_SIZE - 1)) {
		len1 = LOG_BUFFER_SIZE - lb->tail;
		len2 = printed - len1;
	} else {
		len1 = printed;
		len2 = 0;
	}

	if (len1)
		memcpy(lb->buf + lb->tail, buf, len1);

	if (len2)
		memcpy(lb->buf, buf + len1, len2);

	lb->total += printed;

	if (len2)
		lb->head = lb->tail = len2;
	else {
		lb->tail += len1;
		if (lb->total > LOG_BUFFER_SIZE)
			lb->head = lb->tail;
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

	if (uart_init_done) {
		for(i = 0; i < printed; i++)
			uart_putc(*buf++);
	}

	spin_unlock(&log_buffer.buffer_lock);

	return printed;
}
