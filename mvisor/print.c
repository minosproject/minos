#include <mvisor/types.h>
#include <mvisor/varlist.h>
#include <mvisor/string.h>
#include <mvisor/print.h>
#include <mvisor/spinlock.h>
#include <config/config.h>
#include <drivers/uart.h>

#define PRINTF_DEC		0X0001
#define PRINTF_HEX		0x0002
#define PRINTF_OCT		0x0004
#define PRINTF_BIN		0x0008
#define PRINTF_MASK		(0x0f)
#define PRINTF_UNSIGNED		0X0010
#define PRINTF_SIGNED		0x0020

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

void vmm_log_init(void)
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

int numbric(char *buf, unsigned long num, int flag)
{
	int len = 0;

	switch (flag & PRINTF_MASK) {
		case PRINTF_DEC:
			if (flag &PRINTF_SIGNED)
				len = itoa(buf, (signed long)num);
			else
				len = uitoa(buf, num);
			break;
		case PRINTF_HEX:
			len = hextoa(buf, num);
			break;
		case PRINTF_OCT:
			len = octtoa(buf, num);
			break;
		case PRINTF_BIN:
			len = bintoa(buf, num);
			break;
		default:
			break;
	}

	return len;
}

/*
 *for this function if the size over than 1024
 *this will cause an overfolow error,we will fix
 *this bug later
 */
int vsprintf(char *buf, const char *fmt, va_list arg)
{
	char *str;
	int len;
	char *tmp;
	signed long number;
	unsigned long unumber;
	int flag = 0;

	if (buf == NULL)
		return -1;

	for (str = buf; *fmt; fmt++) {

		if (*fmt != '%') {
			*str++ = *fmt;
			continue;
		}

		fmt++;
		switch (*fmt) {
			case 'd':
				flag |= PRINTF_DEC | PRINTF_SIGNED;
				break;
			case 'x':
				flag |= PRINTF_HEX | PRINTF_UNSIGNED;
				break;
			case 'u':
				flag |= PRINTF_DEC | PRINTF_UNSIGNED;
				break;
			case 's':
				len = strlen(tmp = va_arg(arg, char *));
				strncpy(str, tmp, len);
				str += len;
				continue;
				break;
			case 'c':
				*str = (char)(va_arg(arg, int));
				str++;
				continue;
				break;
			case 'o':
				flag |= PRINTF_DEC | PRINTF_SIGNED;
				break;
			case '%':
				*str = '%';
				str++;
				continue;
				break;
			default:
				*str = '%';
				*(str+1) = *fmt;
				str += 2;
				continue;
				break;
		}

		if(flag & PRINTF_UNSIGNED){
			unumber = va_arg(arg, unsigned long);
			len = numbric(str, unumber, flag);
		} else {

			number = va_arg(arg, signed long);
			len = numbric(str, number, flag);
		}
		str+=len;
		flag = 0;
	}

	*str = 0;

	return str-buf;
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

int level_print(const char *fmt, ...)
{
	char ch;
	va_list arg;
	int printed, i;
	char *buf;

	ch = *fmt;
	if (is_digit(ch)) {
		ch = ch - '0';
		if(ch > CONFIG_LOG_LEVEL)
			return 0;
		fmt++;
	}

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
