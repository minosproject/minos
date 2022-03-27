#ifndef __MINOS_ASM_CURRENT_H__
#define __MINOS_ASM_CURRENT_H__

#include <minos/compiler.h>
#include <asm/barrier.h>

static inline void *asm_get_current_task(void)
{
	void *tsk;
	__asm__ volatile ("mov %0, x18" : "=r" (tsk));
	return tsk;
}

static inline void *asm_get_current_task_info(void)
{
	void *tsk_info;
	__asm__ volatile ("mov %0, x18" : "=r" (tsk_info));
	return tsk_info;
}

static inline void asm_set_current_task(void *task)
{
	__asm__ volatile ("mov x18, %0" : : "r" (task));
}

#endif
