#ifndef __MINOS_COMPILER_H_
#define __MINOS_COMPILER_H_

#define __cache_line_size__	(64)

#define __section(S)		__attribute__((__section__(#S)))
#define __used			__attribute__((__used__))
#define __unused		__attribute__((__unused__))
#define __align(x)		__attribute__((__aligned__(x)))
#define __align_cache_line	__align(__cache_line_size__)
#define __packed__		__attribute__((__packed__))

#endif
