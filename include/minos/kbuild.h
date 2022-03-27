#ifndef __MINOS_KBUILD_H__
#define __MINOS_KBUILD_H__

#define DEFINE(sym, val) \
	asm volatile("\n->" #sym " %0 " #val : : "i" (val))

#endif
