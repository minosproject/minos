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
#include <minos/sched.h>
#include <minos/time.h>

struct fifo_vcpu_data {
	struct list_head fifo_list;
	struct vcpu *vcpu;
};

struct fifo_pcpu_data {
	struct list_head ready_list;
	struct list_head sleep_list;
	struct vcpu *idle;
};

static void fifo_set_vcpu_state(struct pcpu *pcpu,
		struct vcpu *vcpu, int state)
{
	unsigned long flags;
	struct fifo_vcpu_data *td = vcpu->sched_data;
	struct fifo_pcpu_data *pd = pcpu->sched_data;

	if (vcpu->is_idle)
		return;

	/*
	 * what will happend if the interrupt is happend
	 * here ? and the interrupt trigger kick_vcpu to
	 * set the vcpu to an other state ?
	 *
	 * for now: the only case is, when the vcpu is called
	 * to go to suspend/idle state, but here a interrupt is
	 * happend and the vcpu is set to ready state, exit
	 * int will set to suspend again. and now the suspend
	 * and idle state is only in reset or idle case.
	 * the finaly state is ok for these two case.
	 */

	local_irq_save(flags);

	/* delete the vcpu from current list */
	if (td->fifo_list.next != NULL)
		list_del(&td->fifo_list);

	if (state == VCPU_STAT_READY)
		list_add(&pd->ready_list, &td->fifo_list);
	else if (state == VCPU_STAT_SUSPEND)
		list_add_tail(&pd->sleep_list, &td->fifo_list);
	else if (state == VCPU_STAT_STOPPED)
		td->fifo_list.next = NULL;
	else
		panic("unsupport vcpu state for fifo sched\n");

	vcpu->state = state;

	local_irq_restore(flags);
}

static struct vcpu *fifo_pick_vcpu(struct pcpu *pcpu)
{
	struct fifo_pcpu_data *pd = pcpu->sched_data;
	struct fifo_vcpu_data *td;

	if (is_list_empty(&pd->ready_list))
		return pd->idle;

	/*
	 * list will never empty, since idle vcpu is
	 * always on the ready list
	 */
	td = (struct fifo_vcpu_data *)
		list_first_entry(&pd->ready_list,
		struct fifo_vcpu_data, fifo_list);

	return td->vcpu;
}

static int fifo_add_vcpu(struct pcpu *pcpu, struct vcpu *vcpu)
{
	unsigned long flags;
	struct fifo_vcpu_data *td = vcpu->sched_data;
	struct fifo_pcpu_data *pd = pcpu->sched_data;

	if (vcpu->is_idle) {
		pd->idle = vcpu;
		return 0;
	}

	local_irq_save(flags);
	td->fifo_list.next = NULL;
	vcpu->state = VCPU_STAT_STOPPED;
	local_irq_restore(flags);

	return 0;
}

static int fifo_remove_vcpu(struct pcpu *pcpu, struct vcpu *vcpu)
{
	/* do nothing */

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

static void fifo_deinit_pcpu_data(struct pcpu *pcpu)
{
	struct fifo_pcpu_data *d;

	d = pcpu->sched_data;
	if (d)
		free(d);

	pcpu->sched_data = NULL;
}

static int fifo_init_vcpu_data(struct pcpu *pcpu, struct vcpu *vcpu)
{
	struct fifo_vcpu_data *data;

	data = (struct fifo_vcpu_data *)
		malloc(sizeof(struct fifo_vcpu_data));
	if (!data)
		return -ENOMEM;

	init_list(&data->fifo_list);
	vcpu->sched_data = data;
	data->vcpu = vcpu;

	return 0;
}

static void fifo_deinit_vcpu_data(struct pcpu *pcpu, struct vcpu *vcpu)
{
	struct fifo_vcpu_data *data;

	data = vcpu->sched_data;
	if (!data)
		return;

	vcpu->sched_data = NULL;
	free(data);
}

static void fifo_sched(struct pcpu *pcpu,
			struct vcpu *c, struct vcpu *n)
{
	unsigned long flags;
	struct fifo_vcpu_data *td = n->sched_data;
	struct fifo_pcpu_data *pd = pcpu->sched_data;

	local_irq_save(flags);

	/*
	 * put the vcpu which will run soon to the
	 * tail of the pcpu's ready list
	 */
	if (!n->is_idle) {
		list_del(&td->fifo_list);
		list_add_tail(&pd->ready_list, &td->fifo_list);
	}

	local_irq_restore(flags);
}

static int fifo_sched_vcpu(struct pcpu *pcpu, struct vcpu *t)
{
	unsigned long flags;
	struct fifo_vcpu_data *td = t->sched_data;
	struct fifo_pcpu_data *pd = pcpu->sched_data;


	local_irq_save(flags);

	/*
	 * put the vcpu to the head of the list
	 */
	list_del(&td->fifo_list);
	list_add(&pd->ready_list, &td->fifo_list);

	local_irq_restore(flags);

	return 1;
}

static unsigned long fifo_tick_handler(struct pcpu *pcpu)
{
	next_vcpu = fifo_pick_vcpu(pcpu);

	return MILLISECS(50);
}

static int fifo_can_idle(struct pcpu *pcpu)
{
	struct fifo_pcpu_data *pd = pcpu->sched_data;

	if (is_list_empty(&pd->ready_list))
		return 1;

	return 0;
}

static struct sched_class sched_fifo = {
	.name			= "fifo",
	.flags			= 0,
	.sched_interval		= MILLISECS(100),
	.set_vcpu_state		= fifo_set_vcpu_state,
	.pick_vcpu		= fifo_pick_vcpu,
	.add_vcpu		= fifo_add_vcpu,
	.remove_vcpu		= fifo_remove_vcpu,
	.init_pcpu_data		= fifo_init_pcpu_data,
	.deinit_pcpu_data	= fifo_deinit_pcpu_data,
	.init_vcpu_data		= fifo_init_vcpu_data,
	.deinit_vcpu_data	= fifo_deinit_vcpu_data,
	.sched			= fifo_sched,
	.sched_vcpu		= fifo_sched_vcpu,
	.tick_handler		= fifo_tick_handler,
	.can_idle		= fifo_can_idle,
};

static int sched_fifo_init(void)
{
	return register_sched_class(&sched_fifo);
}

subsys_initcall(sched_fifo_init);
