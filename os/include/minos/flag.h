#ifndef __MINOS_FLAG_H__
#define __MINOS_FLAG_H__

#include <minos/spinlock.h>

#define FLAG_WAIT_CLR_ALL       0
#define FLAG_WAIT_CLR_AND       0
#define FLAG_WAIT_CLR_ANY       1
#define FLAG_WAIT_CLR_OR        1
#define FLAG_WAIT_SET_ALL       2
#define FLAG_WAIT_SET_AND       2
#define FLAG_WAIT_SET_ANY       3
#define FLAG_WAIT_SET_OR        3

#define FLAG_CONSUME		0x80

#define FLAG_CLR             0
#define FLAG_SET             1

struct flag_grp {
	int type;
	struct list_head wait_list;
	flag_t flags;
	spinlock_t lock;
};

struct flag_node {
	struct list_head list;
	struct task *task;
	void *flag_grp;
	flag_t flags;
	int wait_type;
};

struct flag_grp *flag_create(flag_t flags);
int flag_del(struct flag_grp *grp, int opt);
flag_t flag_accept(struct flag_grp *grp, flag_t flags, int wait_type);

flag_t flag_pend(struct flag_grp *grp, flag_t flags,
		int wait_type, uint32_t timeout);
flag_t flag_pend_get_flags_ready(void);
flag_t flag_post(struct flag_grp *grp, flag_t flags, int opt);

#endif
