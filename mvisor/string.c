#include <mvisor/types.h>
#include <mvisor/string.h>
#include <mvisor/errno.h>

long absolute(long num)
{
	if (num > 0)
		return num;
	return (~num) + 1;
}

long num_to_str(char *buf, unsigned int num, int bdho)
{
	char hex[] ="0123456789abcdef";
	long m, len, res;
	char tmp_buf[64] = {0};
	char *tmp = tmp_buf;

	do {
		m = num % bdho;
		num = num / bdho;
		*tmp++ = hex[m];
	} while (num >= bdho);
	if (num != 0)
		*tmp++ = hex[num];

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

int is_digit(char ch)
{
	return ((ch <= '9') && (ch >= '0'));
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

#if 0

#ifndef CONFIG_HAS_ARCH_STRCHR
char *strchr(char *src, char ch)
{
	for (; *src != (char)ch; ++src)
		if (*src == '\0')
			return NULL;
	return (char *)src;
}
#endif

#ifndef CONFIG_HAS_ARCH_MEMCPY
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
#endif

#ifndef CONFIG_HAS_ARCH_MEMSET
void memset(char *base, char ch, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		*(base + i) = ch;
	}
}
#endif

#endif
