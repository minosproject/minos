#ifndef __MINOS_TASK_H_
#define __MINOS_TASK_H_

#include <minos/list.h>
#include <minos/atomic.h>

#define TASK_NAME_SIZE		(64)

#define TASK_STAT_READY		(0x0000u)
#define TASK_STAT_RUNNING	(0x0001u)
#define TASK_STAT_SEM		(0x0002u)
#define TASK_STAT_MBOX		(0x0004u)
#define TASK_STAT_Q		(0x0008u)
#define TASK_STAT_SUSPEND	(0x0010u)
#define TASK_STAT_MUTEX		(0x0020u)
#define TASK_STAT_FLAG		(0x0040u)
#define TASK_STAT_MULTI		(0x0080u)
#define TASK_STAT_IDLE		(0x0100u)

#define TASK_STAT_PEND_OK	(0x0u)
#define TASK_STAT_PEND_TO	(0x01u)
#define TASK_STAT_PEND_ABORT	(0x02u)

#define TASK_TYPE_NORMAL	(0x00u)
#define TASK_TYPE_VCPU		(0x01u)

#define TASK_FLAG_NONE		(0x0)
#define TASK_FLAG_VCPU		(1ul << 1)

#define TASK_DEFAULT_STACK_SIZE	(SIZE_4K * 2)

#define TASK_IDLE_PR		(511)

struct task;

struct task {
	void *stack_base;
	uint32_t stack_size;
	int32_t pr;
	int pid;
	uint32_t task_delay;
	atomic_t task_stat;
	atomic_t task_pend_stat;
	uint32_t affinity;
	uint8_t task_type;
	uint8_t bit_map_x;
	uint8_t bit_map_y;
	uint8_t res;
	struct list_head list;
	char name[TASK_NAME_SIZE];
	void *pdata;
};

struct task_info {
	char *name;
	void *entry;
	uint32_t stack_size;
	int pr;
	int affinity;
	void *p;
	unsigned long flags;
};

#include <virt/vcpu.h>

#define vcpu_to_gp_regs(vcpu)	(stack_to_gp_regs(vcpu->task->stack_base))

#define DEFINE_TASK(n, e, ss, pr, a, pdata, f)	\
	static const struct task_info __used \
	task_info_##pr __section(.__static_task_info) = { \
		.name = a, \
		.entry = e, \
		.stack_size = ss, \
		.pr = p, \
		.p = pdata, \
		.flags = f, \
	}

struct task *create_task(char *name, void *entry,
		uint32_t stack_size, int pr, int affinity,
		void *p, unsigned long flag);
struct task *create_idle_task(void);
void tasks_init(void);

#endif
