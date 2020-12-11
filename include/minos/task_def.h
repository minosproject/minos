#ifndef __TASK_DEF_H__
#define __TASK_DEF_H__

#include <minos/types.h>
#include <minos/list.h>
#include <minos/atomic.h>
#include <minos/timer.h>

/*
 * 0  - 63 : realtime task
 * 64 - 70 : physical cpu local task, non-realtime task
 * 71	   : idle task
 */

#define OS_NR_TASKS		512
#define OS_REALTIME_TASK	64

#define OS_LOWEST_REALTIME_PRIO	(OS_REALTIME_TASK - 1)
#define OS_PRIO_IDLE		71
#define OS_PRIO_LOWEST		OS_IDLE_PRIO
#define OS_PRIO_VCPU		70
#define OS_PRIO_DEFAULT_0	64
#define OS_PRIO_DEFAULT_1	65
#define OS_PRIO_DEFAULT_2	66
#define OS_PRIO_DEFAULT_3	67
#define OS_PRIO_DEFAULT_4	68
#define OS_PRIO_DEFAULT_5	69
#define OS_PRIO_DEFAULT		OS_PRIO_DEFAULT_3

#define OS_PRIO_PCPU		72
#define OS_RDY_TBL_SIZE		(OS_REALTIME_TASK / 8)
#define OS_LOCAL_PRIO(p)	(p - OS_REALTIME_TASK)
#define OS_INVALID_PRIO		0xff

#define OS_TASK_RESERVED	((struct task *)1)

#define TASK_FLAGS_IDLE_BIT	0
#define TASK_FLAGS_VCPU_BIT	1
#define TASK_FLAGS_REALTIME_BIT	3
#define TASK_FLAGS_32BIT_BIT	4
#define TASK_FLAGS_KERNEL_BIT	5

#define TASK_FLAGS_IDLE		BIT(TASK_FLAGS_IDLE_BIT)
#define TASK_FLAGS_VCPU		BIT(TASK_FLAGS_VCPU_BIT)
#define TASK_FLAGS_PERCPU	BIT(TASK_FLAGS_PERCPU_BIT)
#define TASK_FLAGS_REALTIME	BIT(TASK_FLAGS_REALTIME_BIT)
#define TASK_FLAGS_32BIT	BIT(TASK_FLAGS_32BIT_BIT)
#define TASK_FLAGS_KERNEL	BIT(TASK_FLAGS_KERNEL_BIT)

#define PCPU_AFF_ANY		0xffff
#define PCPU_AFF_LOCAL		0xfffe
#define PCPU_AFF_PERCPU		0xfffd

#define TASK_NAME_SIZE		(31)

#define TASK_STAT_RDY           0x00  /* Ready to run */
#define TASK_STAT_SEM           0x01  /* Pending on semaphore */
#define TASK_STAT_MBOX          0x02  /* Pending on mailbox */
#define TASK_STAT_Q             0x04  /* Pending on queue */
#define TASK_STAT_MUTEX         0x08  /* Pending on mutual exclusion semaphore */
#define TASK_STAT_FLAG          0x10  /* Pending on event flag group */
#define TASK_STAT_MULTI         0x20  /* Pending on multiple events */
#define TASK_STAT_SUSPEND       0x40  /* Task is suspended */
#define TASK_STAT_RUNNING	0x100 /* Task is running */
#define TASK_STAT_STOPPED	0x200

#define TASK_STAT_PEND_ANY      (TASK_STAT_SEM | TASK_STAT_MBOX | TASK_STAT_Q | TASK_STAT_MUTEX | TASK_STAT_FLAG)

#define TASK_STAT_PEND_OK       0u  /* Pending status OK, not pending, or pending complete */
#define TASK_STAT_PEND_TO       1u  /* Pending timed out */
#define TASK_STAT_PEND_ABORT    2u  /* Pending aborted */

#define TASK_STACK_SIZE		CONFIG_TASK_STACK_SIZE

typedef int (*task_func_t)(void *data);
struct flag_node;
struct event;

#define KWORKER_FLAG_MASK	0xffff
#define KWORKER_TASK_RECYCLE	BIT(0)

struct task {
	void *stack_base;
	void *stack_origin;
	uint32_t stack_size;

	uint32_t pid;
	void *udata;

	unsigned long flags;

	/*
	 * link to the global task list or the
	 * cpu task list, and stat list used for
	 * pcpu task to link to the state list.
	 */
	struct list_head list;
	struct list_head stat_list;

	uint32_t delay;
	struct timer_list delay_timer;	

	/*
	 * the spinlock will use to protect the below member
	 * which may modified by different cpu at the same
	 * time:
	 * 1 - stat
	 * 2 - pend_stat
	 */
	spinlock_t lock;
	volatile uint32_t stat;
	volatile uint32_t pend_stat;

	uint8_t prio;
	uint8_t bx;
	uint8_t by;
	uint8_t bitx;
	uint8_t bity;
	uint8_t local_prio;
	uint8_t local_mask;

	atomic_t event_timeout;
	struct event *lock_event;
	struct event *wait_event;

	int flag_rdy;
	struct flag_node *flag_node;

	/*
	 * event_list - list to the event which the task waitting for
	 */
	struct list_head event_list;

	void *msg;
	uint32_t flags_rdy;

	/*
	 * affinity - the cpu node which the task affinity to
	 */
	uint32_t affinity;

	unsigned long run_time;

	/* stat information */
	unsigned long ctx_sw_cnt;
	unsigned long start_ns;

	char name[TASK_NAME_SIZE + 1];

	void *pdata;		/* connect to the vcpu */
	void *arch_data;	/* arch data to this task */
} __align_cache_line;

/*
 * this task_info is stored at the top of the task's
 * stack
 */
struct task_info {
	int cpu;
	int preempt_count;
	unsigned long flags;
	struct task *task;
};

#define TIF_NEED_RESCHED	0
#define TIF_32BIT		1

#define TIF_HARDIRQ_MASK	8
#define TIF_SOFTIRQ_MASK	9

#define __TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define __TIF_32BIT		(1 << TIF_32BIT)

#define __TIF_HARDIRQ_MASK	(1 << TIF_HARDIRQ_MASK)
#define __TIF_SOFTIRQ_MASK	(1 << TIF_SOFTIRQ_MASK)
#define __TIF_IN_INTERRUPT	(0xff00)

#endif
