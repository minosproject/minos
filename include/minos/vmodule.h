#ifndef _MINOS_MODULE_H_
#define _MINOS_MODULE_H_

#include <minos/types.h>
#include <minos/list.h>
#include <minos/task.h>

#include <minos/device_id.h>

#define INVALID_MODULE_ID (0xffff)

struct vmodule {
	char name[32];
	int id;
	struct list_head list;
	uint32_t context_size;
	int (*valid_for_task)(struct task *task);
	void (*state_save)(struct task *task, void *context);
	void (*state_restore)(struct task *task, void *context);
	void (*state_init)(struct task *task, void *context);
	void (*state_deinit)(struct task *task, void *context);
	void (*state_reset)(struct task *task, void *context);
	void (*state_suspend)(struct task *task, void *context);
	void (*state_resume)(struct task *task, void *context);
};

typedef int (*vmodule_init_fn)(struct vmodule *);

int task_vmodules_init(struct task *task);
int task_vmodules_deinit(struct task *task);
int task_vmodules_reset(struct task *task);
void *get_vmodule_data_by_id(struct task *task, int id);
void *get_vmodule_data_by_name(struct task *task, const char *name);
void save_task_vmodule_state(struct task *task);
void restore_task_vmodule_state(struct task *task);
void suspend_task_vmodule_state(struct task *task);
void resume_task_vmodule_state(struct task *task);
int vmodules_init(void);
int register_task_vmodule(const char *name, vmodule_init_fn fn);

#endif
