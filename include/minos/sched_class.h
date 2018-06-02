#ifndef __MINOS_SCHED_CLASS_H__
#define __MINOS_SCHED_CLASS_H__

struct task;
struct pcpu;

struct sched_class {
	char *name;
	struct task *(*pick_task)(struct pcpu *);
	void (*set_task_state)(struct pcpu *, struct task *, int);
	int (*add_task)(struct pcpu *, struct task *);
	int (*suspend_task)(struct pcpu *, struct task *);
	int (*init_pcpu_data)(struct pcpu *);
	int (*init_task_data)(struct pcpu *, struct task *k);
	void (*sched)(struct pcpu *, struct task *, struct task *);
	void (*sched_task)(struct pcpu *, struct task *);
	struct task *(*sched_new)(struct pcpu *);
};

struct sched_class *get_sched_class(char *name);
int register_sched_class(struct sched_class *cls);

#endif
