#ifndef __ASM_ATOMIC_H__
#define __ASM_ATOMIC_H__

#include <asm/aarch64_common.h>
#include <minos/types.h>
#include <asm/barrier.h>

static inline int atomic_read(atomic_t *t)
{
	smp_mb();
	return t->value;
}

static inline void atomic_set(int i, atomic_t *t)
{
	t->value = i;
	smp_mb();
}

#ifndef CONFIG_ARM_ATOMIC_LSE

static inline void atomic_add(int i, atomic_t *v)
{
	unsigned long tmp;
	int ret;

	asm volatile(
"1:	ldxr	%w0, %2\n"
"	add	%w0, %w0, %w3\n"
"	stxr	%w1, %w0, %2\n"
"	cbnz	%w1, 1b"
	: "=&r" (ret), "=&r" (tmp), "+Q" (v->value)
	: "Ir" (i)
	);
}

static inline int atomic_add_return(int i, atomic_t *v)
{
	unsigned long tmp;
	int ret;

	asm volatile(
"1:	ldxr	%w0, %2\n"
"	add	%w0, %w0, %w3\n"
"	stlxr	%w1, %w0, %2\n"
"	cbnz	%w1, 1b"
	: "=&r" (ret), "=&r" (tmp), "+Q" (v->value)
	: "Ir" (i)
	: "memory"
	);

	smp_mb();
	return ret;
}

static inline int atomic_add_return_old(int i, atomic_t *v)
{
	unsigned long tmp;
	int ret;

	asm volatile(
"1:	ldxr	%w0, %2\n"
"	add	%w0, %w0, %w3\n"
"	stlxr	%w1, %w0, %2\n"
"	cbnz	%w1, 1b\n"
"	sub	%w0, %w0, %w3"
	: "=&r" (ret), "=&r" (tmp), "+Q" (v->value)
	: "Ir" (i)
	: "memory"
	);

	smp_mb();
	return ret;
}

static inline void atomic_sub(int i, atomic_t *v)
{
	unsigned long tmp;
	int ret;

	asm volatile(
"1:	ldxr	%w0, %2\n"
"	sub	%w0, %w0, %w3\n"
"	stlxr	%w1, %w0, %2\n"
"	cbnz	%w1, 1b"
	: "=&r" (ret), "=&r" (tmp), "+Q" (v->value)
	: "Ir" (i)
	);
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
	unsigned long tmp;
	int ret;

	asm volatile(
"1:	ldaxr	%w0, %2\n"
"	sub	%w0, %w0, %w3\n"
"	stlxr	%w1, %w0, %2\n"
"	cbnz	%w1, 1b"
	: "=&r" (ret), "=&r" (tmp), "+Q" (v->value)
	: "Ir" (i)
	: "memory"
	);

	smp_mb();
	return ret;
}

static inline int atomic_sub_return_old(int i, atomic_t *v)
{
	unsigned long tmp;
	int ret;

	asm volatile(
"1:	ldaxr	%w0, %2\n"
"	sub	%w0, %w0, %w3\n"
"	stlxr	%w1, %w0, %2\n"
"	cbnz	%w1, 1b\n"
"	add	%w0, %w0, %w3"
	: "=&r" (ret), "=&r" (tmp), "+Q" (v->value)
	: "Ir" (i)
	: "memory"
	);

	return ret;
}

static inline int atomic_cmpxchg(atomic_t *t, int old, int new)
{
	unsigned long tmp;
	int oldval;

	smp_mb();

	asm volatile(
"1:	ldxr	%w1, %2\n"
"	cmp	%w1, %w3\n"
"	b.ne	2f\n"
"	stxr	%w0, %w4, %2\n"
"	cbnz	%w0, 1b\n"
"2:			"
	: "=&r" (tmp), "=&r" (oldval), "+Q" (t->value)
	: "Ir" (old), "r" (new)
	: "cc"
	);

	smp_mb();
	return oldval;
}

static inline int atomic_inc_if_postive(atomic_t *t)
{
	unsigned long tmp;
	int oldval;
	int value;

	smp_mb();

	asm volatile(
"1:	ldxr	%w1, %2\n"
"	cmp	%w1, #0\n"
"	b.lt	2f\n"
"	add	%w3, %w1, #1\n"
"	stxr	%w0, %w3, %2\n"
"	cbnz	%w0, 1b\n"
"2:			"
	: "=&r" (tmp), "=&r" (oldval), "+Q" (t->value), "=&r" (value)
	:
	: "cc"
	);

	smp_mb();
	return oldval;
}

static inline int atomic_dec_set_negtive_if_zero(atomic_t *t)
{
	unsigned long tmp;
	int oldval;
	int value;

	smp_mb();

	asm volatile(
"1:	ldxr	%w1, %2\n"
"	cmp	%w1, #0\n"
"	b.le	2f\n"
"	sub	%w3, %w1, #1\n"
"	cmp	%w3, #0\n"
"	b.ne	3f\n"
"	sub	%w3, %w3, #1\n"	// set the value to -1 if new value is -1
"3:		"
"	stxr	%w0, %w3, %2\n"
"	cbnz	%w0, 1b\n"
"2:			"
	: "=&r" (tmp), "=&r" (oldval), "+Q" (t->value), "=&r" (value)
	:
	: "cc"
	);

	smp_mb();
	return oldval;
}

static inline int atomic_cmpsub(atomic_t *t, int old, int value)
{
	unsigned long tmp;
	int oldval;

	smp_mb();

	asm volatile(
"1:	ldxr	%w1, %2\n"
"	cmp	%w1, %w3\n"
"	sub	%w4, %w1, %4\n"
"	b.eq	2f\n"
"	stxr	%w0, %w4, %2\n"
"	cbnz	%w0, 1b\n"
"2:			"
	: "=&r" (tmp), "=&r" (oldval), "+Q" (t->value)
	: "Ir" (old), "r" (value)
	: "cc"
	);

	smp_mb();
	return oldval;
}

#else

#define ATOMIC_OP(op, asm_op)				\
static inline void atomic_##op(int i, atomic_t *t)	\
{							\
	register int w0 asm ("w0") = i;			\
	register atomic_t *x1 asm ("x1") = t;		\
							\
	asm volatile (					\
"	" #asm_op "	%w[i], %[v]\n"			\
	: [i] "+r" (w0), [v] "+Q" (t->value)		\
	: "r" (x1)					\
	: "x16", "x17", "x30"				\
	);						\
}

ATOMIC_OP(andnot, stclr)
ATOMIC_OP(or, stset)
ATOMIC_OP(xor, steor)
ATOMIC_OP(add, stadd)

#define ATOMIC_OP_ADD_RETURN(name, mb, cl...)				\
static inline int atomic_add_return##name(int i, atomic_t *v)		\
{									\
	register int w0 asm ("w0") = i;					\
	register atomic_t *x1 asm ("x1") = v;				\
									\
	asm volatile(							\
	"	ldadd" #mb "	%w[i], w30, %[v]\n"			\
	"	add	%w[i], %w[i], w30"				\
	: [i] "+r" (w0), [v] "+Q" (v->value)				\
	: "r" (x1)							\
	: "x16", "x17", "x30", ##cl					\
	);								\
									\
	return w0;							\
}

static inline int atomic_add_return_old(int i, atomic_t *v)
{
	register int w0 asm ("w0") = i;
	register atomic_t *x1 asm ("x1") = v;

	asm volatile(
"	ldaddal %w[i], w30, %[v]\n"
"	add	%w[i], %w[i], w30"
	: [i] "+r" (w0), [v] "+Q" (v->value)
	: "r" (x1)
	: "x16", "x17", "x30", "memory"
	);

	return w0;
}

ATOMIC_OP_ADD_RETURN(_relaxed,   )
ATOMIC_OP_ADD_RETURN(_acquire,  a, "memory")
ATOMIC_OP_ADD_RETURN(_release,  l, "memory")
ATOMIC_OP_ADD_RETURN(        , al, "memory")

static inline void atomic_sub(int i, atomic_t *v)
{
	register int w0 asm ("w0") = i;
	register atomic_t *x1 asm ("x1") = v;

	asm volatile(
	"	neg	%w[i], %w[i]\n"
	"	stadd	%w[i], %[v]"
	: [i] "+&r" (w0), [v] "+Q" (v->value)
	: "r" (x1)
	: "x16", "x17", "x30"
	);
}

#define ATOMIC_OP_SUB_RETURN(name, mb, cl...)				\
static inline int atomic_sub_return##name(int i, atomic_t *v)		\
{									\
	register int w0 asm ("w0") = i;					\
	register atomic_t *x1 asm ("x1") = v;				\
									\
	asm volatile(							\
"	neg	%w[i], %w[i]\n"						\
"	ldadd" #mb "	%w[i], w30, %[v]\n"				\
"	add	%w[i], %w[i], w30"					\
	: [i] "+&r" (w0), [v] "+Q" (v->value)				\
	: "r" (x1)							\
	: "x16", "x17", "x30", ##cl);					\
									\
	return w0;							\
}

ATOMIC_OP_SUB_RETURN(_relaxed,   )
ATOMIC_OP_SUB_RETURN(_acquire,  a, "memory")
ATOMIC_OP_SUB_RETURN(_release,  l, "memory")
ATOMIC_OP_SUB_RETURN(        , al, "memory")

static inline int atomic_sub_return_old(int i, atomic_t *v)
{
	register int w0 asm ("w0") = i;
	register atomic_t *x1 asm ("x1") = v;

	asm volatile(
"	neg	%w[i], %w[i]\n"
"	ldaddal %w[i], w30, %[v]"
	: [i] "+&r" (w0), [v] "+Q" (v->value)
	: "r" (x1)
	: "x16", "x17", "x30", "memory");

	return w0;
}

#define __CMPXCHG_CASE(w, sz, name, mb, cl...)				\
static inline unsigned long __cmpxchg_case_##name(volatile void *ptr,	\
						  unsigned long old,	\
						  unsigned long new)	\
{									\
	register unsigned long x0 asm ("x0") = (unsigned long)ptr;	\
	register unsigned long x1 asm ("x1") = old;			\
	register unsigned long x2 asm ("x2") = new;			\
									\
	asm volatile(							\
"	mov	" #w "30, %" #w "[old]\n"				\
"	cas" #mb #sz "\t" #w "30, %" #w "[new], %[v]\n"			\
"	mov	%" #w "[ret], " #w "30"					\
	: [ret] "+r" (x0), [v] "+Q" (*(unsigned long *)ptr)		\
	: [old] "r" (x1), [new] "r" (x2)				\
	: "x16", "x17", "x30", ##cl);					\
									\
	return x0;							\
}

__CMPXCHG_CASE(w, b,     1,   )
__CMPXCHG_CASE(w, h,     2,   )
__CMPXCHG_CASE(w,  ,     4,   )
__CMPXCHG_CASE(x,  ,     8,   )
__CMPXCHG_CASE(w, b, acq_1,  a, "memory")
__CMPXCHG_CASE(w, h, acq_2,  a, "memory")
__CMPXCHG_CASE(w,  , acq_4,  a, "memory")
__CMPXCHG_CASE(x,  , acq_8,  a, "memory")
__CMPXCHG_CASE(w, b, rel_1,  l, "memory")
__CMPXCHG_CASE(w, h, rel_2,  l, "memory")
__CMPXCHG_CASE(w,  , rel_4,  l, "memory")
__CMPXCHG_CASE(x,  , rel_8,  l, "memory")
__CMPXCHG_CASE(w, b,  mb_1, al, "memory")
__CMPXCHG_CASE(w, h,  mb_2, al, "memory")
__CMPXCHG_CASE(w,  ,  mb_4, al, "memory")
__CMPXCHG_CASE(x,  ,  mb_8, al, "memory")

#endif // CONFIG_ARM_ATOMIC_LSE

#endif
