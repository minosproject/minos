#ifndef _INIT_H
#define _INIT_H

#define INIT_PATH_SIZE		128

#define CMDLINE_TAG		"xxoo"
#define ARCH_NAME_SIZE		8
#define BOARD_NAME_SIZE		16

#define MEM_MAX_REGION		16

struct cmdline {
	unsigned long tag;
	unsigned long head;
	char arg[256];
};

typedef int (*init_call)(void);

#define __init_data __attribute__((section(".__init_data_section")))
#define __init_text __attribute__((section(".__init_text")))

#define __init_0	__attribute__((section(".__init_func_0")))
#define __init_1	__attribute__((section(".__init_func_1")))
#define __init_2	__attribute__((section(".__init_func_2")))
#define __init_3	__attribute__((section(".__init_func_3")))
#define __init_4	__attribute__((section(".__init_func_4")))
#define __init_5	__attribute__((section(".__init_func_5")))
#define __init_6	__attribute__((section(".__init_func_6")))

#define early_initcall(fn)	\
	static init_call __init_call_##fn __init_0 = fn

#define arch_initcall(fn)	\
	static init_call __init_call_##fn __init_1 = fn

#define platform_initcall(fn)	\
	static init_call __init_call_##fn __init_2 = fn

#define fs_initcall(fn)	\
	static init_call __init_call_##fn __init_3 = fn

#define device_initcall(fn)	\
	static init_call __init_call_##fn __init_4 = fn

#define late_initcall(fn)	\
	static init_call __init_call_##fn __init_5 = fn

#define __section(S) __attribute__ ((__section__(#S)))

#endif
