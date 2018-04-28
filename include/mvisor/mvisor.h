#ifndef _MVISOR_MVISOR_H_
#define _MVISOR_MVISOR_H_

#include <mvisor/types.h>
#include <mvisor/string.h>
#include <mvisor/print.h>
#include <mvisor/mm.h>
#include <mvisor/list.h>
#include <mvisor/spinlock.h>
#include <mvisor/errno.h>
#include <mvisor/panic.h>
#include <mvisor/smp.h>
#include <mvisor/varlist.h>
#include <mvisor/vcpu.h>

#define BUG_ON(condition)	\
	if ((condition)) {	\
		do { ; } while (1); \
	}

typedef void (*hook_func_t)(struct vcpu *vcpu, void *data);

enum mvisor_hook_type {
	MVISOR_HOOK_TYPE_EXIT_FROM_GUEST = 0,
	MVISOR_HOOK_TYPE_ENTER_TO_GUEST,
	MVISOR_HOOK_TYPE_CREATE_VM,
	MVISOR_HOOK_TYPE_UNKNOWN,
};

struct mvisor_hook {
	hook_func_t fn;
	void *data;
	struct list_head list;
};

void mvisor_exit_from_guest(struct vcpu *vcpu);
void mvisor_enter_to_guest(struct vcpu *vcpu);
int mvisor_register_hook(hook_func_t fn,
	void *data, enum mvisor_hook_type type);

#endif
