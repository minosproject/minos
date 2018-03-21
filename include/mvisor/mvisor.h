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

typedef int (*hook_func_t)(vcpu_t *vcpu, void *data);

enum vmm_hook_type {
	VMM_HOOK_TYPE_EXIT_FROM_GUEST = 0,
	VMM_HOOK_TYPE_ENTER_TO_GUEST,
	VMM_HOOK_TYPE_CREATE_VM,
	VMM_HOOK_TYPE_UNKNOWN,
};

struct vmm_hook {
	hook_func_t fn;
	void *data;
	struct list_head list;
};

void vmm_exit_from_guest(vcpu_t *vcpu);
void vmm_enter_to_guest(vcpu_t *vcpu);
int vmm_register_hook(hook_func_t fn,
	void *data, enum vmm_hook_type type);

#endif
