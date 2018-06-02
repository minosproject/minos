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
