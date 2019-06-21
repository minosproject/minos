/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <minos/vmodule.h>
#include <minos/init.h>
#include <minos/mm.h>
#include <minos/task.h>
#include <minos/spinlock.h>

extern unsigned char __vmodule_start;
extern unsigned char __vmodule_end;

static int vmodule_class_nr = 0;
static LIST_HEAD(vmodule_list);

static struct vmodule *create_vmodule(struct module_id *id)
{
	struct vmodule *vmodule;
	vmodule_init_fn fn;

	vmodule = malloc(sizeof(*vmodule));
	if (!vmodule) {
		return NULL;
	}

	memset(vmodule, 0, sizeof(*vmodule));
	strncpy(vmodule->name, id->name, sizeof(vmodule->name) - 1);
	init_list(&vmodule->list);
	vmodule->id = vmodule_class_nr++;

	/* call init routine */
	if (id->data) {
		fn = (vmodule_init_fn)id->data;
		fn(vmodule);
	}

	list_add(&vmodule_list, &vmodule->list);
	return vmodule;
}

int register_task_vmodule(const char *name, vmodule_init_fn fn)
{
	struct vmodule *vmodule;
	struct module_id mid = { .name = name, .comp = NULL, .data = fn };

	vmodule = create_vmodule(&mid);
	if (!vmodule) {
		pr_err("create vmodule %s failed\n", name);
		return -ENOMEM;
	}

	return 0;
}

void *get_vmodule_data_by_id(struct task *task, int id)
{
	return task->context[id];
}

void *get_vmodule_data_by_name(struct task *task, const char *name)
{
	struct vmodule *vmodule;
	int id = INVALID_MODULE_ID;

	list_for_each_entry(vmodule, &vmodule_list, list) {
		if (strcmp(vmodule->name, name) == 0) {
			id = vmodule->id;
			break;
		}
	}

	if (id != INVALID_MODULE_ID)
		return task->context[id];

	return NULL;
}

int task_vmodules_init(struct task *task)
{
	struct list_head *list;
	struct vmodule *vmodule;
	void *data;
	int size;

	/*
	 * firset allocate memory to store each vmodule
	 * context's context data
	 */
	size = vmodule_class_nr * sizeof(void *);
	task->context = malloc(size);
	if (!task->context)
		panic("No more memory for task vmodule cotnext\n");

	memset(task->context, 0, size);

	list_for_each(&vmodule_list, list) {
		vmodule = list_entry(list, struct vmodule, list);
		if (vmodule->context_size) {
			/* for the vcpu task some context is not necessary */
			if (vmodule->valid_for_task &&
					!vmodule->valid_for_task(task))
				continue;

			/* for reboot if memory is areadly allocated skip it */
			data = task->context[vmodule->id];
			if (!data) {
				data = malloc(vmodule->context_size);
				task->context[vmodule->id] = data;
			}

			memset(data, 0, vmodule->context_size);
			if (vmodule->state_init)
				vmodule->state_init(task, data);
		}
	}

	return 0;
}

int task_vmodules_deinit(struct task *task)
{
	struct vmodule *vmodule;
	void *data;

	list_for_each_entry(vmodule, &vmodule_list, list) {
		data = task->context[vmodule->id];
		if (vmodule->state_deinit && data)
			vmodule->state_deinit(task, data);

		if (data)
			free(data);
	}

	return 0;
}

int task_vmodules_reset(struct task *task)
{
	struct vmodule *vmodule;
	void *data;

	list_for_each_entry(vmodule, &vmodule_list, list) {
		data = task->context[vmodule->id];
		if (vmodule->state_reset && data)
			vmodule->state_reset(task, data);
	}

	return 0;
}

void restore_task_vmodule_state(struct task *task)
{
	struct vmodule *vmodule;
	void *context;

	list_for_each_entry(vmodule, &vmodule_list, list) {
		context = get_vmodule_data_by_id(task, vmodule->id);
		if (vmodule->state_restore && context)
			vmodule->state_restore(task, context);
	}
}

void save_task_vmodule_state(struct task *task)
{
	struct vmodule *vmodule;
	void *context;

	list_for_each_entry(vmodule, &vmodule_list, list) {
		context = get_vmodule_data_by_id(task, vmodule->id);
		if (vmodule->state_save && context)
			vmodule->state_save(task, context);
	}
}

void suspend_task_vmodule_state(struct task *task)
{
	struct vmodule *vmodule;
	void *context;

	list_for_each_entry(vmodule, &vmodule_list, list) {
		context = get_vmodule_data_by_id(task, vmodule->id);
		if (vmodule->state_suspend && context)
			vmodule->state_suspend(task, context);
	}
}

void resume_task_vmodule_state(struct task *task)
{
	struct vmodule *vmodule;
	void *context;

	list_for_each_entry(vmodule, &vmodule_list, list) {
		context = get_vmodule_data_by_id(task, vmodule->id);
		if (vmodule->state_resume && context)
			vmodule->state_resume(task, context);
	}
}

int vmodules_init(void)
{
	struct module_id *mid;
	struct vmodule *vmodule;

	section_for_each_item(__vmodule_start, __vmodule_end, mid) {
		vmodule = create_vmodule(mid);
		if (!vmodule)
			pr_err("create vmodule %s failed\n", mid->name);
	}

	return 0;
}
