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
#include <minos/sched.h>

#define MAX_SCHED_CLASS		(8)

static int index = 0;
static struct sched_class *sched_classes[MAX_SCHED_CLASS];

int register_sched_class(struct sched_class *cls)
{
	int i;
	struct sched_class *c;

	if ((!cls) || (index >= MAX_SCHED_CLASS))
		return -EINVAL;

	for (i = 0; i < index; i++) {
		c = sched_classes[i];
		if (!c)
			continue;

		if (!(strcmp(c->name, cls->name)))
			return -EEXIST;
	}

	sched_classes[index] = cls;
	index++;

	return 0;
}

struct sched_class *get_sched_class(char *name)
{
	int i;
	struct sched_class *c = NULL;
	struct sched_class *dc = NULL;

	for (i = 0; i < MAX_SCHED_CLASS; i++) {
		c = sched_classes[i];
		if (!c)
			continue;

		if (!strcmp(c->name, name))
			return c;

		if (!strcmp(c->name, "fifo"))
			dc = c;
	}

	return dc;
}
