#ifndef _STRING_H
#define _STRING_H

#include <mvisor/types.h>

long absolute(long num);
long num_to_str(char *buf, unsigned int num, int bdho);
long itoa(char *buf, long num);
long uitoa(char *buf, unsigned long num);
long hextoa(char *buf, unsigned long num);
long octtoa(char *buf, unsigned long num);
long bintoa(char *buf, unsigned long num);
int strlen(char *buf);
char *strcpy(char *des, char *src);
char *strncpy(char *des, char *src, int len);
int is_digit(char ch);
int strcmp(const char *src, const char *dst);
int memcmp(const char *src, const char *dst, size_t size);
int strncmp(const char *src, const char *dst, int n);
char *strchr(char *src, char ch);
int memcpy(void *target, void *source, int size);
extern void memset(char *base, char ch, int size);

#endif
