/*
 * Based on arch/arm/include/asm/cmpxchg.h
 *
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_CMPXCHG_H__
#define __ASM_CMPXCHG_H__

#ifndef CONFIG_ARM_ATOMIC_LSE
#define __XCHG_CASE(w, sz, name, mb, nop_lse, acq, acq_lse, rel, cl)	\
static inline unsigned long __xchg_case_##name(unsigned long x,		\
					       volatile void *ptr)	\
{									\
	unsigned long ret, tmp;						\
									\
	asm volatile (							\
	"	prfm	pstl1strm, %2\n"				\
	"1:	ld" #acq "xr" #sz "\t%" #w "0, %2\n"			\
	"	st" #rel "xr" #sz "\t%w1, %" #w "3, %2\n"		\
	"	cbnz	%w1, 1b\n"					\
	"	" #mb							\
	: "=&r" (ret), "=&r" (tmp), "+Q" (*(u8 *)ptr)			\
	: "r" (x)							\
	: cl);								\
									\
	return ret;							\
}

#define __CMPXCHG_CASE(w, sz, name, mb, acq, rel, cl)			\
static inline unsigned long __cmpxchg_case_##name(volatile void *ptr,	\
				     unsigned long old,			\
				     unsigned long new)			\
{									\
	unsigned long tmp, oldval;					\
									\
	asm volatile(							\
	"	prfm	pstl1strm, %[v]\n"				\
	"1:	ld" #acq "xr" #sz "\t%" #w "[oldval], %[v]\n"		\
	"	eor	%" #w "[tmp], %" #w "[oldval], %" #w "[old]\n"	\
	"	cbnz	%" #w "[tmp], 2f\n"				\
	"	st" #rel "xr" #sz "\t%w[tmp], %" #w "[new], %[v]\n"	\
	"	cbnz	%w[tmp], 1b\n"					\
	"	" #mb "\n"						\
	"	mov	%" #w "[oldval], %" #w "[old]\n"		\
	"2:"								\
	: [tmp] "=&r" (tmp), [oldval] "=&r" (oldval),			\
	  [v] "+Q" (*(unsigned long *)ptr)				\
	: [old] "Lr" (old), [new] "r" (new)				\
	: cl);								\
									\
	return oldval;							\
}									\

__CMPXCHG_CASE(w, b,     1,        ,  ,  ,         )
__CMPXCHG_CASE(w, h,     2,        ,  ,  ,         )
__CMPXCHG_CASE(w,  ,     4,        ,  ,  ,         )
__CMPXCHG_CASE( ,  ,     8,        ,  ,  ,         )
__CMPXCHG_CASE(w, b, acq_1,        , a,  , "memory")
__CMPXCHG_CASE(w, h, acq_2,        , a,  , "memory")
__CMPXCHG_CASE(w,  , acq_4,        , a,  , "memory")
__CMPXCHG_CASE( ,  , acq_8,        , a,  , "memory")
__CMPXCHG_CASE(w, b, rel_1,        ,  , l, "memory")
__CMPXCHG_CASE(w, h, rel_2,        ,  , l, "memory")
__CMPXCHG_CASE(w,  , rel_4,        ,  , l, "memory")
__CMPXCHG_CASE( ,  , rel_8,        ,  , l, "memory")
__CMPXCHG_CASE(w, b,  mb_1, dmb ish,  , l, "memory")
__CMPXCHG_CASE(w, h,  mb_2, dmb ish,  , l, "memory")
__CMPXCHG_CASE(w,  ,  mb_4, dmb ish,  , l, "memory")
__CMPXCHG_CASE( ,  ,  mb_8, dmb ish,  , l, "memory")

#else
#define __XCHG_CASE(w, sz, name, mb, nop_lse, acq, acq_lse, rel, cl)	\
static inline unsigned long __xchg_case_##name(unsigned long x,		\
					       volatile void *ptr)	\
{									\
	unsigned long ret;						\
									\
	asm volatile (							\
	"	nop\n"							\
	"	nop\n"							\
	"	swp" #acq_lse #rel #sz "\t%" #w "3, %" #w "0, %2\n"	\
	"	nop\n"							\
	"	" #nop_lse						\
	: "=&r" (ret), "=&r" (tmp), "+Q" (*(u8 *)ptr)			\
	: "r" (x)							\
	: cl);								\
									\
	return ret;							\
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
	asm volatile (							\
	"	mov	" #w "30, %" #w "[old]\n"			\
	"	cas" #mb #sz "\t" #w "30, %" #w "[new], %[v]\n"		\
	"	mov	%" #w "[ret], " #w "30")			\
	: [ret] "+r" (x0), [v] "+Q" (*(unsigned long *)ptr)		\
	: [old] "r" (x1), [new] "r" (x2)				\
	: "x30" , ##cl;							\
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

__XCHG_CASE(w, b,     1,        ,    ,  ,  ,  ,         )
__XCHG_CASE(w, h,     2,        ,    ,  ,  ,  ,         )
__XCHG_CASE(w,  ,     4,        ,    ,  ,  ,  ,         )
__XCHG_CASE( ,  ,     8,        ,    ,  ,  ,  ,         )
__XCHG_CASE(w, b, acq_1,        ,    , a, a,  , "memory")
__XCHG_CASE(w, h, acq_2,        ,    , a, a,  , "memory")
__XCHG_CASE(w,  , acq_4,        ,    , a, a,  , "memory")
__XCHG_CASE( ,  , acq_8,        ,    , a, a,  , "memory")
__XCHG_CASE(w, b, rel_1,        ,    ,  ,  , l, "memory")
__XCHG_CASE(w, h, rel_2,        ,    ,  ,  , l, "memory")
__XCHG_CASE(w,  , rel_4,        ,    ,  ,  , l, "memory")
__XCHG_CASE( ,  , rel_8,        ,    ,  ,  , l, "memory")
__XCHG_CASE(w, b,  mb_1, dmb ish, nop,  , a, l, "memory")
__XCHG_CASE(w, h,  mb_2, dmb ish, nop,  , a, l, "memory")
__XCHG_CASE(w,  ,  mb_4, dmb ish, nop,  , a, l, "memory")
__XCHG_CASE( ,  ,  mb_8, dmb ish, nop,  , a, l, "memory")

#undef __XCHG_CASE

#define __XCHG_GEN(sfx)							\
static inline unsigned long __xchg##sfx(unsigned long x,		\
					volatile void *ptr,		\
					int size)			\
{									\
	switch (size) {							\
	case 1:								\
		return __xchg_case##sfx##_1(x, ptr);			\
	case 2:								\
		return __xchg_case##sfx##_2(x, ptr);			\
	case 4:								\
		return __xchg_case##sfx##_4(x, ptr);			\
	case 8:								\
		return __xchg_case##sfx##_8(x, ptr);			\
	default:							\
		return 0;						\
	}								\
}

__XCHG_GEN()
__XCHG_GEN(_acq)
__XCHG_GEN(_rel)
__XCHG_GEN(_mb)

#undef __XCHG_GEN

#define __xchg_wrapper(sfx, ptr, x)					\
({									\
	__typeof__(*(ptr)) __ret;					\
	__ret = (__typeof__(*(ptr)))					\
		__xchg##sfx((unsigned long)(x), (ptr), sizeof(*(ptr))); \
	__ret;								\
})

/* xchg */
#define xchg_relaxed(...)	__xchg_wrapper(    , __VA_ARGS__)
#define xchg_acquire(...)	__xchg_wrapper(_acq, __VA_ARGS__)
#define xchg_release(...)	__xchg_wrapper(_rel, __VA_ARGS__)
#define xchg(...)		__xchg_wrapper( _mb, __VA_ARGS__)

#define __CMPXCHG_GEN(sfx)						\
static inline unsigned long __cmpxchg##sfx(volatile void *ptr,		\
					   unsigned long old,		\
					   unsigned long new,		\
					   int size)			\
{									\
	switch (size) {							\
	case 1:								\
		return __cmpxchg_case##sfx##_1(ptr, (u8)old, new);	\
	case 2:								\
		return __cmpxchg_case##sfx##_2(ptr, (u16)old, new);	\
	case 4:								\
		return __cmpxchg_case##sfx##_4(ptr, old, new);		\
	case 8:								\
		return __cmpxchg_case##sfx##_8(ptr, old, new);		\
	default:							\
		return 0;						\
	}								\
}

__CMPXCHG_GEN()
__CMPXCHG_GEN(_acq)
__CMPXCHG_GEN(_rel)
__CMPXCHG_GEN(_mb)

#undef __CMPXCHG_GEN

#define __cmpxchg_wrapper(sfx, ptr, o, n)				\
({									\
	__typeof__(*(ptr)) __ret;					\
	__ret = (__typeof__(*(ptr)))					\
		__cmpxchg##sfx((ptr), (unsigned long)(o),		\
				(unsigned long)(n), sizeof(*(ptr)));	\
	__ret;								\
})

/* cmpxchg */
#define cmpxchg_relaxed(...)	__cmpxchg_wrapper(    , __VA_ARGS__)
#define cmpxchg_acquire(...)	__cmpxchg_wrapper(_acq, __VA_ARGS__)
#define cmpxchg_release(...)	__cmpxchg_wrapper(_rel, __VA_ARGS__)
#define cmpxchg(...)		__cmpxchg_wrapper( _mb, __VA_ARGS__)
#define cmpxchg_local		cmpxchg_relaxed

/* cmpxchg64 */
#define cmpxchg64_relaxed	cmpxchg_relaxed
#define cmpxchg64_acquire	cmpxchg_acquire
#define cmpxchg64_release	cmpxchg_release
#define cmpxchg64		cmpxchg
#define cmpxchg64_local		cmpxchg_local

#endif
