#ifndef __MINOS_CALLTRACE_H_
#define __MINOS_CALLTRACE_H_

#include <minos/compiler.h>

void __panic(gp_regs *regs, char *str, ...) __noreturn;
void print_symbol(unsigned long addr);
void dump_stack(gp_regs *regs, unsigned long *stack);

#define panic(...)	__panic(NULL, __VA_ARGS__)

#define WARN_ON(condition, ...)		 \
	if ((condition)) {		 \
		do {			 \
			pr_err("WARN: " __VA_ARGS__); \
		} while (0); 		 \
	}

#define BUG_ON(condition, ...)		 \
	if ((condition)) {		 \
		do {			 \
			panic("BUG: " __VA_ARGS__); \
		} while (0); 		 \
	}

#define ASSERT(condition)		 \
	if (!(condition)) {		 \
		do {			 \
			panic("ASSERT FAIL: %s %d\n", __func__, __LINE__); \
		} while (0); 		 \
	}

#endif
