#ifndef _MINOS_MINOS_H_
#define _MINOS_MINOS_H_

#include <minos/types.h>
#include <minos/string.h>
#include <minos/print.h>
#include <minos/list.h>
#include <minos/spinlock.h>
#include <minos/smp.h>
#include <config/config.h>
#include <minos/errno.h>
#include <minos/init.h>
#include <minos/arch.h>
#include <minos/calltrace.h>
#include <minos/time.h>
#include <minos/preempt.h>
#include <minos/hook.h>
#include <minos/current.h>

#define section_for_each_item_addr(__start_addr, __end_addr, __var)            \
	size_t _i, _cnt;                                                       \
	unsigned long _base, _end;                                             \
	_base = __start_addr;                                                  \
	_end = __end_addr;                                                     \
	_cnt = (_end - _base) / sizeof(*(__var));                              \
	__var = (__typeof__(__var))(_base);                                    \
	for (_i = 0; _i < _cnt; ++_i, ++(__var))

#define section_for_each_item(__start, __end, __var)                           \
	section_for_each_item_addr((unsigned long)&(__start),                  \
				    (unsigned long)&(__end), __var)

extern spinlock_t __kernel_lock;

void __might_sleep(const char *file, int line, int preempt_offset);

static inline int taken_from_guest(gp_regs *regs)
{
	return arch_taken_from_guest(regs);
}

#ifdef CONFIG_OS_REALTIME_CORE0
#define kernel_lock_irqsave(flags)	local_irq_save(flags)
#define kernel_unlock_irqrestore(flags) local_irq_restore(flags)
#define kernel_lock()
#define kernel_unlock()
#else
#define kernel_lock_irqsave(flags)	spin_lock_irqsave(&__kernel_lock, flags)
#define kernel_unlock_irqrestore(flags) spin_unlock_irqrestore(&__kernel_lock, flags)
#define kernel_lock()			raw_spin_lock(&__kernel_lock)
#define kernel_unlock()			raw_spin_unlock(&__kernel_lock)
#endif

#define WARN(condition, format...) ({						\
	int __ret_warn_on = !!(condition);				\
	if (unlikely(__ret_warn_on))					\
		pr_warn(format);					\
	unlikely(__ret_warn_on);					\
})

#define WARN_ONCE(condition, format...)	({			\
	static bool __section(.data.unlikely) __warned;		\
	int __ret_warn_once = !!(condition);			\
								\
	if (unlikely(__ret_warn_once))				\
		if (WARN(!__warned, format)) 			\
			__warned = true;			\
	unlikely(__ret_warn_once);				\
})

#define might_sleep() \
	do { \
		__might_sleep(__FILE__, __LINE__, 0); \
	} while (0)

#endif
