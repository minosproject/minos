#ifndef __MINOS_HOOK_H__
#define __MINOS_HOOK_H__

typedef int (*hook_func_t)(void *item, void *contex);

enum hook_type {
	MINOS_HOOK_TYPE_EXIT_FROM_GUEST = 0,
	MINOS_HOOK_TYPE_ENTER_TO_GUEST,
	OS_HOOK_TYPE_CREATE_VM,
	MINOS_HOOK_TYPE_CREATE_VM_VDEV,
	OS_HOOK_TYPE_DESTROY_VM,
	MINOS_HOOK_TYPE_SUSPEND_VM,
	MINOS_HOOK_TYPE_RESUME_VM,
	MINOS_HOOK_TYPE_ENTER_IRQ,
	OS_HOOK_TASK_SWITCH_TO,
	OS_HOOK_CREATE_TASK,
	MINOS_HOOK_TYPE_UNKNOWN,
};

struct hook {
	hook_func_t fn;
	struct list_head list;
};

int do_hooks(void *item, void *context, enum hook_type type);
int register_hook(hook_func_t fn, enum hook_type type);

#endif
