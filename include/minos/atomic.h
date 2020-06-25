#ifndef _MINOS_ATOMIC_H_
#define _MINOS_ATOMIC_H_

#include <asm/atomic.h>

#define ATOMIC_INIT(v) { (v) }

static inline void atomic_inc(atomic_t *t)
{
	atomic_add(1, t);
}

static inline void atomic_dec(atomic_t *t)
{
	atomic_sub(1, t);
}

static inline int atomic_inc_return(atomic_t *t)
{
	return atomic_add_return(1, t);
}

static inline int atomic_dec_return(atomic_t *t)
{
	return atomic_sub_return(1, t);
}

static inline int atomic_inc_return_old(atomic_t *t)
{
	return atomic_add_return_old(1, t);
}

static inline int atomic_dec_return_old(atomic_t *t)
{
	return atomic_sub_return_old(1, t);
}

static inline int atomic_inc_and_test(atomic_t *t)
{
	return atomic_add_return(1, t) == 0;
}

static inline int atomic_dec_and_test(atomic_t *t)
{
	return atomic_sub_return(1, t) == 0;
}

static inline int atomic_add_negative(int i, atomic_t *t)
{
	return atomic_add_return(i, t) < 0;
}

#endif
