#ifndef __MINOS_CALLTRACE_H_
#define __MINOS_CALLTRACE_H_

void panic(char *str, ...);
void print_symbol(unsigned long addr);
void dump_stack(gp_regs *regs, unsigned long *stack);

#define BUG_ON(condition)		 \
	if ((condition)) {		 \
		do {			 \
			panic("BUG_ON"); \
		} while (1); 		 \
	}

#endif
