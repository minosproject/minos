#ifndef __TASK_DEF_H__
#define __TASK_DEF_H__

#include <minos/types.h>
#include <minos/task_info.h>
#include <minos/list.h>
#include <minos/atomic.h>
#include <minos/timer.h>
#include <asm/tcb.h>

#ifdef CONFIG_TASK_STACK_SIZE
#define TASK_STACK_SIZE	CONFIG_TASK_STACK_SIZE
#else
#define TASK_STACK_SIZE (2 * PAGE_SIZE)
#endif

#ifndef CONFIG_NR_TASKS
#define CONFIG_NR_TASKS 256
#endif

#define OS_NR_TASKS CONFIG_NR_TASKS

#define OS_PRIO_MAX		8
#define OS_PRIO_DEFAULT_0	0
#define OS_PRIO_DEFAULT_1	1
#define OS_PRIO_DEFAULT_2	2
#define OS_PRIO_DEFAULT_3	3
#define OS_PRIO_DEFAULT_4	4
#define OS_PRIO_DEFAULT_5	5
#define OS_PRIO_DEFAULT_6	6
#define OS_PRIO_DEFAULT_7	7

#define OS_PRIO_REALTIME	OS_PRIO_DEFAULT_0
#define OS_PRIO_SYSTEM		OS_PRIO_DEFAULT_3
#define OS_PRIO_VCPU		OS_PRIO_DEFAULT_4
#define OS_PRIO_DEFAULT		OS_PRIO_DEFAULT_5
#define OS_PRIO_IDLE		OS_PRIO_DEFAULT_7
#define OS_PRIO_LOWEST		OS_PRIO_IDLE

#define TASK_FLAGS_VCPU			BIT(0)
#define TASK_FLAGS_REALTIME		BIT(1)
#define TASK_FLAGS_IDLE			BIT(2)
#define TASK_FLAGS_NO_AUTO_START	BIT(3)
#define TASK_FLAGS_32BIT		BIT(4)
#define TASK_FLAGS_PERCPU		BIT(5)

#define TASK_AFF_ANY		(-1)
#define TASK_NAME_SIZE		(32)

#define TASK_STAT_RUNNING 0x00
#define TASK_STAT_WAIT_EVENT 0x01
#define TASK_STAT_WAKING 0x02
#define TASK_STAT_SUSPEND 0x04
#define TASK_STAT_STOP 0x08

#define KWORKER_FLAG_MASK 0xffff
#define KWORKER_TASK_RECYCLE BIT(0)

enum {
	TASK_EVENT_NULL = 0,

	TASK_EVENT_IRQ,
	TASK_EVENT_VIRQ,

	TASK_EVENT_MBOX,
	TASK_EVENT_Q,
	TASK_EVENT_MUTEX,
	TASK_EVENT_FLAG,
	TASK_EVENT_SEM,
	TASK_EVENT_TIMER,

	TASK_EVENT_STARTUP,

	TASK_EVENT_ANY,
};

#define TASK_STAT_PEND_OK       0u  /* Pending status OK, not pending, or pending complete */
#define TASK_STAT_PEND_TO       1u  /* Pending timed out */
#define TASK_STAT_PEND_ABORT    2u  /* Pending aborted */

#define KWORKER_FLAG_MASK	0xffff
#define KWORKER_TASK_RECYCLE	BIT(0)

#define TASK_TIMEOUT_CLEAR	0x0
#define TASK_TIMEOUT_REQUESTED	0x1
#define TASK_TIMEOUT_TRIGGER	0x2

#define TASK_REQ_FLUSH_TLB	(1 << 0)
#define TASK_REQ_STOP		(1 << 1)

typedef int (*task_func_t)(void *data);

struct process;

#ifdef CONFIG_VIRT
struct vcpu;
#endif

struct task {
	struct task_info ti;

	void *stack_base;
	void *stack_top;
	void *stack_bottom;

	/*
	 * pid - process id
	 * tid - task id
	 * hid - handle id
	 */
	int pid;
	int tid;

	unsigned long flags;

	struct list_head proc_list;	// link to the process list, if is a thread.
	struct list_head stat_list;	// link to the sched list used for sched.

	uint32_t delay;
	struct timer_list delay_timer;

	/*
	 * the next task belongs to the same process
	 */
	struct task *next;
	struct process *proc;

	/*
	 * the spinlock will use to protect the below member
	 * which may modified by different cpu at the same
	 * time:
	 * 1 - stat
	 * 2 - pend_stat
	 */
	spinlock_t s_lock;
	int stat;
	long pend_stat;
	long request;

	int wait_type;			// which event is task waitting for.
	void *msg;			// used for mbox to pass data
	unsigned long wait_event;	// the event instance which the task is waitting.
	struct list_head event_list;

	struct flag_node *flag_node;	// used for the flag event.
	long flags_rdy;

	/*
	 * affinity - the cpu node which the task affinity to
	 */
	int cpu;
	int last_cpu;
	int affinity;
	int prio;

	unsigned long run_time;

	unsigned long ctx_sw_cnt;	// switch count of this task.
	unsigned long start_ns;		// when the task started last time.

	char name[TASK_NAME_SIZE];

	void (*exit_from_user)(struct task *task, gp_regs *regs);
	void (*return_to_user)(struct task *task, gp_regs *regs);

	union {
		void *pdata;			// the private data of this task, such as vcpu.
#ifdef CONFIG_VIRT
		struct vcpu *vcpu;
#endif
	};

	struct cpu_context cpu_context;
} __cache_line_align;

#define OS_TASK_RESERVED	((struct task *)1)

#endif
