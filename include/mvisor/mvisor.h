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

int (*hook_func_t)(vcpu_t *vcpu, void *data);

enum vmm_hook_type {
	VMM_HOOK_TYPE_EXIT_GUEST = 0,
	VMM_HOOK_TYPE_RESUME_VMM,
	VMM_HOOK_TYPE_UNKNOWN,
};

struct vmm_hook {
	hook_func_t fn;
	void *data;
	struct list_head list;
};

int vmm_exit_from_guest(vcpu_t *vcpu);

int vmm_register_hook(hook_func_t fn, void *data, enum vmm_hook_type type);

#endif
