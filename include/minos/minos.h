#ifndef _MINOS_MINOS_H_
#define _MINOS_MINOS_H_

#include <minos/types.h>
#include <minos/string.h>
#include <minos/print.h>
#include <minos/mm.h>
#include <minos/list.h>
#include <minos/spinlock.h>
#include <minos/panic.h>
#include <minos/smp.h>
#include <minos/varlist.h>
#include <config/config.h>
#include <minos/errno.h>
#include <minos/init.h>
#include <minos/device_id.h>
#include <minos/arch.h>

struct vcpu;

#define BUG_ON(condition)	\
	if ((condition)) {	\
		do { ; } while (1); \
	}

typedef void (*hook_func_t)(struct vcpu *vcpu, void *contex);

enum hook_type {
	MINOS_HOOK_TYPE_EXIT_FROM_GUEST = 0,
	MINOS_HOOK_TYPE_ENTER_TO_GUEST,
	MINOS_HOOK_TYPE_CREATE_VM,
	MINOS_HOOK_TYPE_UNKNOWN,
};

struct hook {
	hook_func_t fn;
	struct list_head list;
};

void irq_enter(gp_regs *regs);
void irq_exit(gp_regs *regs);

int do_hooks(struct vcpu *vcpu, void *context,
		enum hook_type type);

void *get_module_pdata(unsigned long s, unsigned long e,
		int (*check)(struct module_id *module));
int register_hook(hook_func_t fn, enum hook_type type);


#endif
