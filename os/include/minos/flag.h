#ifndef __MINOS_FLAG_H__
#define __MINOS_FLAG_H__

typedef uint64_t flag_t;

#define FLAG_WAIT_CLR_ALL       0
#define FLAG_WAIT_CLR_AND       0
#define FLAG_WAIT_CLR_ANY       1
#define FLAG_WAIT_CLR_OR        1
#define FLAG_WAIT_SET_ALL       2
#define FLAG_WAIT_SET_AND       2
#define FLAG_WAIT_SET_ANY       3
#define FLAG_WAIT_SET_OR        3

#define FLAG_CONSUME		0x80

#define OS_FLAG_CLR             0
#define OS_FLAG_SET             1


struct flag_grp {
	int type;
	struct list_head wait_list;
	flag_t flags;
	ticketlock_t lock;
};

struct flag_node {
	struct list_head list;
	struct task *task;
	void *flag_grp;
	flag_t flags;
	int wait_type;
};

#endif
