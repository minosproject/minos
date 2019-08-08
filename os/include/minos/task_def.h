#ifndef __TASK_DEF_H__
#define __TASK_DEF_H__

#include <minos/types.h>
#include <minos/list.h>
#include <minos/atomic.h>
#include <minos/timer.h>

#ifdef CONFIG_VIRT
struct vm;
struct vcpu;
#endif

/* the max realtime task will be 64 */
#define OS_NR_TASKS		512
#define OS_REALTIME_TASK	64

#define OS_LOWEST_PRIO		(OS_REALTIME_TASK - 1)
#define OS_PRIO_PCPU		(OS_LOWEST_PRIO + 1)
#define OS_PRIO_IDLE		(OS_LOWEST_PRIO + 2)

#define OS_RDY_TBL_SIZE		(OS_REALTIME_TASK / 8)

#define OS_TASK_RESERVED	((struct task *)1)

#define TASK_FLAGS_IDLE		0x0001
#define TASK_FLAGS_VCPU		0x0002
#define TASK_FLAGS_PERCPU	0x0004

#define PCPU_AFF_NONE		0xffff
#define PCPU_AFF_PERCPU		0xfffe

#define TASK_DEF_STACK_SIZE	(4096)

#define TASK_NAME_SIZE		(31)

#define TASK_STAT_RDY           0x00  /* Ready to run */
#define TASK_STAT_SEM           0x01  /* Pending on semaphore */
#define TASK_STAT_MBOX          0x02  /* Pending on mailbox */
#define TASK_STAT_Q             0x04  /* Pending on queue */
#define TASK_STAT_SUSPEND       0x08  /* Task is suspended */
#define TASK_STAT_MUTEX         0x10  /* Pending on mutual exclusion semaphore */
#define TASK_STAT_FLAG          0x20  /* Pending on event flag group */
#define TASK_STAT_MULTI         0x80  /* Pending on multiple events */
#define TASK_STAT_RUNNING	0x100 /* Task is running */
#define TASK_STAT_STOPPED	0x200

#define TASK_STAT_PEND_ANY      (TASK_STAT_SEM | TASK_STAT_MBOX | TASK_STAT_Q | TASK_STAT_MUTEX | TASK_STAT_FLAG)

#define TASK_STAT_PEND_OK       0u  /* Pending status OK, not pending, or pending complete */
#define TASK_STAT_PEND_TO       1u  /* Pending timed out */
#define TASK_STAT_PEND_ABORT    2u  /* Pending aborted */

#define TASK_DEFAULT_STACK_SIZE CONFIG_TASK_STACK_SIZE

typedef void (*task_func_t)(void *data);
struct flag_node;
struct event;

struct task {
	void *stack_base;
	void *stack_origin;
	uint32_t stack_size;
	void *udata;
	int pid;

	unsigned long flags;

	/*
	 * link to the global task list or the
	 * cpu task list, and stat list used for
	 * pcpu task to link to the state list.
	 */
	struct list_head list;
	struct list_head stat_list;
	struct list_head event_list;

	void *msg;
	uint32_t flags_rdy;

	uint32_t delay;
	struct timer_list delay_timer;

	volatile uint16_t stat;
	volatile uint16_t pend_stat;
	uint8_t del_req;
	uint8_t prio;
	uint8_t bx;
	uint8_t by;
	prio_t bitx;
	prio_t bity;

	/* the event that this task hold currently */
	atomic_t event_timeout;
	struct event *lock_event;
	struct event *wait_event;

	/* used to the flag type */
	int flag_rdy;
	struct flag_node *flag_node;

	/*
	 * affinity - the cpu node which the task affinity to
	 * cpu - the cpu node which the task runing at currently
	 */
	uint16_t affinity;
	uint16_t cpu;

	unsigned long run_time;
	unsigned long start_ns;

	spinlock_t lock;

	/* stat information */
	unsigned long ctx_sw_cnt;
	unsigned long cycle_total;
	unsigned long cycle_start;
	void *stack_current;
	uint32_t stack_used;

	char name[TASK_NAME_SIZE + 1];

#ifdef CONFIG_VIRT
	struct vcpu *vcpu;
	struct vm *vm;
#endif
	void *pdata;
	void *arch_data;	/* arch data to this task */
	void **context;
} __align_cache_line;

/*
 * this task_info is stored at the top of the task's
 * stack
 */
struct task_info {
	int cpu;
	int preempt_count;
	struct task *task;
};

struct task_desc {
	char *name;
	task_func_t func;
	void *arg;
	prio_t prio;
	uint16_t aff;
	uint32_t stk_size;
	unsigned long flags;
};

struct task_event {
	int id;
	struct task *task;
#define TASK_EVENT_EVENT_READY		0x0
#define TASK_EVENT_FLAG_READY		0x1
	int action;
	void *msg;
	uint32_t msk;
	uint32_t delay;
	flag_t flags;
};

#endif
