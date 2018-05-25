#ifndef _MINOS_ATOMIC_H_
#define _MINOS_ATOMIC_H_

typedef struct {
	int value;
} atomic_t;

#define ATOMIC_INIT(v) { (v) }

void atomic_add(int i, atomic_t *t);
void atomic_sub(int i, atomic_t *t);
int atomic_add_return(int i, atomic_t *t);
int atomic_sub_return(int i, atomic_t *t);

static inline int atomic_read(atomic_t *t)
{
	return *(volatile int *)&t->value;
}

static inline void atomic_set(atomic_t *t, int i)
{
	t->value = i;
}

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
