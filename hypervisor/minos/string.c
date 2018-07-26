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
#include <minos/string.h>
#include <minos/errno.h>
#include <minos/varlist.h>

#define PRINTF_DEC		0X0001
#define PRINTF_HEX		0x0002
#define PRINTF_OCT		0x0004
#define PRINTF_BIN		0x0008
#define PRINTF_POINTER		0x0010
#define PRINTF_MASK		(0xff)
#define PRINTF_UNSIGNED		0X0100
#define PRINTF_SIGNED		0x0200

long absolute(long num)
{
	if (num > 0)
		return num;
	return (~num) + 1;
}

long num_to_str(char *buf, unsigned long num, int b)
{
	char hex[] ="0123456789abcdef";
	long m, len, res;
	char tmp_buf[64];
	char *tmp = tmp_buf;
	int bdho = b;

	if (bdho == 32)
		bdho = 16;

	memset(tmp_buf, '0', 64);

	do {
		m = num % bdho;
		num = num / bdho;
		*tmp++ = hex[m];
	} while (num >= bdho);
	if (num != 0)
		*tmp++ = hex[num];

	if (b == 32)
		res = len = 16;
	else
		res = len = tmp - tmp_buf;

	while (len > 0) {
		*buf++ = tmp_buf[len-1];
		len--;
	}

	return res;
}

long itoa(char *buf, long num)
{
	int len = 0;
	int tmp = 0;

	if (buf == NULL)
		return -1;

	if (num < 0) {
		num = absolute(num);
		*buf++ = '-';
		tmp = 1;
	}
	len = num_to_str(buf, num, 10);

	return len + tmp;
}

long uitoa(char *buf, unsigned long num)
{
	return num_to_str(buf, num, 10);
}

long hextoa(char *buf, unsigned long num)
{
	return num_to_str(buf, num, 16);
}

long octtoa(char *buf, unsigned long num)
{
	return num_to_str(buf, num, 8);
}

long bintoa(char *buf, unsigned long num)
{
	return num_to_str(buf, num, 2);
}

long ptoa(char *buf, unsigned long num)
{
	return num_to_str(buf, num, 32);
}

char *strncpy(char *des, char *src, int len)
{
	char *tmp = des;
	int i;

	if (des == NULL || src == NULL)
		return NULL;

	for (i = 0; i < len; i++) {
		des[i] = src[i];
	}

	return tmp;
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
		case PRINTF_POINTER:
			len = ptoa(buf, num);
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
			case 'p':
				flag |= PRINTF_POINTER | PRINTF_UNSIGNED;
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


int sprintf(char *str, const char *format, ...)
{
	va_list arg;
	int count;

	va_start(arg, format);
	count = vsprintf(str, format, arg);
	va_end(arg);

	return count;
}

#if 0
int strlen(char *buf)
{
	int len = 0;

	if (buf == NULL)
		return -1;

	while (*buf++) {
		len++;
	}

	return len;
}

char *strcpy(char *des, char *src)
{
	char *tmp = des;

	if (des == NULL || src == NULL)
		return NULL;

	while ((*des++=*src++) != '\0');

	return tmp;
}

int strcmp(const char *src, const char *dst)
{
	int ret = 0;

	while (!(ret = *(unsigned char *)src - *(unsigned char *)dst) && *dst) {
		++src, ++dst;
	}

	if (ret < 0)
		ret = -1;
	else if (ret > 0)
		ret = 1;

	return (ret);
}

int memcmp(const char *src, const char *dst, size_t size)
{
	int i;
	char ret;

	if (size == 0)
		return -EINVAL;

	for (i = 0; i < size; i++) {
		if (src[i] != dst[i]) {
			ret = src[i] - dst[i];
			if (ret < 0)
				return -1;
			else if (ret > 0)
				return 1;
		}
	}

	return 0;
}

int strncmp(const char *src, const char *dst, int n)
{
	int ret = 0;

	while (n && (!(ret = *(unsigned char *)src - *(unsigned char *)dst))) {
		++src, ++dst;
		n--;
	}

	if (ret < 0)
		ret = -1;
	else if (ret > 0)
		ret = 1;

	return (ret);
}

char *strchr(char *src, char ch)
{
	for (; *src != (char)ch; ++src)
		if (*src == '\0')
			return NULL;
	return (char *)src;
}

int memcpy(void *target, void *source, int size)
{
	char *t = (char *)target;
	char *s = (char *)source;
	int old_size = size;

	if(size <= 0)
		return 0;

	while (size--)
		*t++ = *s++;

	return old_size;
}

void memset(char *base, char ch, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		*(base + i) = ch;
	}
}
#endif
