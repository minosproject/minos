#ifndef __MINOS_VSPACE_H__
#define __MINOS_VSPACE_H__

#include <minos/types.h>
#include <minos/list.h>

struct vspace {
	pgd_t *pgdp;
	spinlock_t lock;
};

#endif
