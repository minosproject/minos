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
#include <common/hypervisor.h>

#define section_for_each_item_addr(__start_addr, __end_addr, __var)            \
	size_t _i, _cnt;                                                       \
	unsigned long _base, _end;                                             \
	_base = __start_addr;                                                  \
	_end = __end_addr;                                                     \
	_cnt = (_end - _base) / sizeof(*(__var));                              \
	__var = (__typeof__(__var))(_base);                                    \
	for (_i = 0; _i < _cnt; ++_i, ++(__var))

#define section_for_each_item(__start, __end, __var)                           \
	section_for_each_item_addr ((unsigned long)&(__start),                 \
				    (unsigned long)&(__end), __var)

struct vcpu;

typedef int (*hook_func_t)(void *item, void *contex);

enum hook_type {
	MINOS_HOOK_TYPE_EXIT_FROM_GUEST = 0,
	MINOS_HOOK_TYPE_ENTER_TO_GUEST,
	MINOS_HOOK_TYPE_CREATE_VM,
	MINOS_HOOK_TYPE_CREATE_VM_VDEV,
	MINOS_HOOK_TYPE_DESTROY_VM,
	MINOS_HOOK_TYPE_SUSPEND_VM,
	MINOS_HOOK_TYPE_RESUME_VM,
	MINOS_HOOK_TYPE_UNKNOWN,
};

struct hook {
	hook_func_t fn;
	struct list_head list;
};

void irq_enter(gp_regs *regs);
void irq_exit(gp_regs *regs);

int do_hooks(void *item, void *context, enum hook_type type);
int register_hook(hook_func_t fn, enum hook_type type);

static inline int taken_from_guest(gp_regs *regs)
{
	return arch_taken_from_guest(regs);
}

static inline void exit_from_guest(struct vcpu *vcpu, gp_regs *regs)
{
	do_hooks((void *)vcpu, (void *)regs,
			MINOS_HOOK_TYPE_EXIT_FROM_GUEST);
}

static inline void enter_to_guest(struct vcpu *vcpu, gp_regs *regs)
{
	do_hooks((void *)vcpu, (void *)regs,
			MINOS_HOOK_TYPE_ENTER_TO_GUEST);
}

#endif
