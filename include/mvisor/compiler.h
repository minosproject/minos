#ifndef __MVISOR_COMPILER_H_
#define __MVISOR_COMPILER_H_

#define __section(S) 	__attribute__((__section__(#S)))
#define __used 		__attribute__((__used__))
#define __unused	__attribute__((__unused__))
#define __align(x)	__attribute__((__aligned__(x)))

#endif
