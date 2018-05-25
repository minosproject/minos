#ifndef _MINOS_VARLIST_H_
#define _MINOS_VARLIST_H_

#include <stdarg.h>


/*
 * offset(n)		 get the offset align of the stack for different arch
 * va_start(ap,n)	 to get the second var of the function in the stack
 * va_end		 just a indicate of the va_list operation ending.
 * va_arg(ap,type)	 get the value of the var,
 */

#if 0
typedef char *va_list;
#define addr(n)		(&n)
#define offset(n) \
	((sizeof(n) + sizeof(unsigned long) - 1) & ~(sizeof(unsigned long) - 1))

#define va_start(ap, n) \
	((ap) = ((char *)(&n)) + offset(n))

#define va_end(ap)	(void)0

#define va_arg(ap, type) \
	(*(type *)((ap) += offset(type), (ap) - offset(type)))
#define	va_start(ap, last) \
	__builtin_va_start((ap), (last))

#define	va_arg(ap, type) \
	__builtin_va_arg((ap), type)

#define	__va_copy(dest, src) \
	__builtin_va_copy((dest), (src))
#endif

#endif
