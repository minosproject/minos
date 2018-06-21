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
#include <minos/sched_class.h>
#include <minos/task.h>
#include <minos/sched.h>
#include <minos/time.h>

struct fifo_task_data {
	struct list_head fifo_list;
	struct task *task;
};

struct fifo_pcpu_data {
	struct list_head ready_list;
	struct list_head sleep_list;
	struct task *idle;
};

static void fifo_set_task_state(struct pcpu *pcpu,
		struct task *task, int state)
{
	unsigned long flags;
	struct fifo_task_data *td = task_to_sched_data(task);
	struct fifo_pcpu_data *pd = pcpu_to_sched_data(pcpu);

	if (task->is_idle)
		return;

	local_irq_save(flags);

	if (state == TASK_STAT_READY) {
		list_del(&td->fifo_list);
		list_add_tail(&pd->ready_list, &td->fifo_list);

		/* if the need_resched flag is set clear it */
		task->resched = 0;
	} else if ((state == TASK_STAT_SUSPEND) ||
		state == TASK_STAT_IDLE) {
		list_del(&td->fifo_list);
		list_add_tail(&pd->sleep_list, &td->fifo_list);
	}

	task->state = state;

	local_irq_restore(flags);
}

static struct task *fifo_pick_task(struct pcpu *pcpu)
{
	struct fifo_pcpu_data *pd = pcpu_to_sched_data(pcpu);
	struct fifo_task_data *td;

	if (is_list_empty(&pd->ready_list))
		return pd->idle;

	/*
	 * list will never empty, since idle task is
	 * always on the ready list
	 */
	td = (struct fifo_task_data *)
		list_first_entry(&pd->ready_list,
		struct fifo_task_data, fifo_list);

	return td->task;
}

static int fifo_add_task(struct pcpu *pcpu, struct task *task)
{
	unsigned long flags;
	struct fifo_task_data *td = task_to_sched_data(task);
	struct fifo_pcpu_data *pd = pcpu_to_sched_data(pcpu);

	if (task->is_idle) {
		pd->idle = task;
		return 0;
	}

	local_irq_save(flags);
	list_add_tail(&pd->sleep_list, &td->fifo_list);
	task->state = TASK_STAT_SUSPEND;
	local_irq_restore(flags);

	return 0;
}

static int fifo_suspend_task(struct pcpu *pcpu, struct task *task)
{
	/* more things TBD */
	fifo_set_task_state(pcpu, task, TASK_STAT_SUSPEND);

	return 0;
}

static int fifo_init_pcpu_data(struct pcpu *pcpu)
{
	struct fifo_pcpu_data *d;

	d = (struct fifo_pcpu_data *)
		malloc(sizeof(struct fifo_pcpu_data));
	if (!d)
		return -ENOMEM;

	init_list(&d->ready_list);
	init_list(&d->sleep_list);
	pcpu->sched_data = d;

	return 0;
}

static int fifo_init_task_data(struct pcpu *pcpu, struct task *task)
{
	struct fifo_task_data *data;

	data = (struct fifo_task_data *)
		malloc(sizeof(struct fifo_task_data));
	if (!data)
		return -ENOMEM;

	init_list(&data->fifo_list);
	task->sched_data = data;
	data->task = task;

	return 0;
}

static void fifo_sched(struct pcpu *pcpu,
			struct task *c, struct task *n)
{
	unsigned long flags;
	struct fifo_task_data *td = task_to_sched_data(n);
	struct fifo_pcpu_data *pd = pcpu_to_sched_data(pcpu);


	local_irq_save(flags);

	/*
	 * put the task which will run soon to the
	 * tail of the pcpu's ready list
	 */
	if (!n->is_idle) {
		list_del(&td->fifo_list);
		list_add_tail(&pd->ready_list, &td->fifo_list);
	}

	local_irq_restore(flags);
}

static void fifo_sched_task(struct pcpu *pcpu, struct task *t)
{
	unsigned long flags;
	struct fifo_task_data *td = task_to_sched_data(t);
	struct fifo_pcpu_data *pd = pcpu_to_sched_data(pcpu);


	local_irq_save(flags);

	/*
	 * put the task to the head of the list
	 */
	list_del(&td->fifo_list);
	list_add(&pd->ready_list, &td->fifo_list);

	local_irq_restore(flags);
}

static struct task *fifo_sched_new(struct pcpu *pcpu)
{
	return fifo_pick_task(pcpu);
}

static struct sched_class sched_fifo = {
	.name		= "fifo",
	.sched_interval = MILLISECS(50),
	.set_task_state = fifo_set_task_state,
	.pick_task	= fifo_pick_task,
	.add_task	= fifo_add_task,
	.suspend_task	= fifo_suspend_task,
	.init_pcpu_data = fifo_init_pcpu_data,
	.init_task_data = fifo_init_task_data,
	.sched		= fifo_sched,
	.sched_task	= fifo_sched_task,
	.sched_new	= fifo_sched_new,
};

static int sched_fifo_init(void)
{
	return register_sched_class(&sched_fifo);
}

subsys_initcall(sched_fifo_init);
