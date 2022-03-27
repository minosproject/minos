#ifndef __MINOS_CONST_H__
#define __MINOS_CONST_H__

#ifdef __ASSEMBLY__
#define _AC(X,Y)        X
#define _AT(T,X)        X
#else
#define __AC(X,Y)       (X##Y)
#define _AC(X,Y)        __AC(X,Y)
#define _AT(T,X)        ((T)(X))
#endif

#define _UL(x)          (_AC(x, UL))
#define _ULL(x)         (_AC(x, ULL))

#define _BITUL(x)       (_UL(1) << (x))
#define _BITULL(x)      (_ULL(1) << (x))

#define UL(x)           (_UL(x))
#define ULL(x)          (_ULL(x))

#endif
