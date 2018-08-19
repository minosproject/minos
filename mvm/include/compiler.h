#ifndef __MVM_COMPILER_H__
#define __MVM_COMPILER_H__

#define __section(S)		__attribute__((__section__(#S)))
#define __used			__attribute__((__used__))
#define __unused		__attribute__((__unused__))
#define __align(x)		__attribute__((__aligned__(x)))
#define __align_cache_line	__align(__cache_line_size__)
#define __packed		__attribute__((__packed__))

#define container_of(ptr, name, member) \
	(name *)((unsigned char *)ptr - ((unsigned char *)&(((name *)0)->member)))

#endif
