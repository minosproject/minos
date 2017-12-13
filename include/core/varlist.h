#ifndef _MVISOR_VARLIST_H_
#define _MVISOR_VARLIST_H_

typedef char *va_list;

/*
 * offset(n)		 get the offset align of the stack for different arch
 * va_start(ap,n)	 to get the second var of the function in the stack
 * va_end		 just a indicate of the va_list operation ending.
 * va_arg(ap,type)	 get the value of the var,
 */

#define addr(n)		(&n)
#define offset(n)	((sizeof(n) + sizeof(unsigned long) - 1) & ~(sizeof(unsigned long) - 1))
#define va_start(ap,n)  ap = (char *)addr(n) + offset(n)
#define va_end(ap)	(void)0
#define va_arg(ap,type) (*(type *)((ap += offset(type)) - offset(type)))

#endif
