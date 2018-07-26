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

#define __init_data 	__section(".__init_data_section")
#define __init_text 	__section(".__init_text")

#define __init_0	__section(".__init_func_0")
#define __init_1	__section(".__init_func_1")
#define __init_2	__section(".__init_func_2")
#define __init_3	__section(".__init_func_3")
#define __init_4	__section(".__init_func_4")
#define __init_5	__section(".__init_func_5")
#define __init_6	__section(".__init_func_6")
#define __init_7	__section(".__init_func_7")
#define __init_8	__section(".__init_func_8")
#define __init_9	__section(".__init_func_9")

#define __define_initcall(fn, id)	\
	static init_call __init_call_##fn __used __init_##id = fn

#define early_initcall(fn) 		__define_initcall(fn, 0)
#define arch_initcall(fn) 		__define_initcall(fn, 1)
#define subsys_initcall(fn) 		__define_initcall(fn, 2)
#define module_initcall(fn) 		__define_initcall(fn, 3)
#define device_initcall(fn)		__define_initcall(fn, 4)
#define early_initcall_percpu(fn) 	__define_initcall(fn, 5)
#define arch_initcall_percpu(fn)	__define_initcall(fn, 6)
#define subsys_initcall_percpu(fn)	__define_initcall(fn, 7)
#define module_initcall_percpu(fn)		__define_initcall(fn, 8)
#define device_initcall_percpu(fn)	__define_initcall(fn, 9)

void arch_init(void);
void early_init(void);
void subsys_init(void);
void module_init(void);
void device_init(void);
void early_init_percpu(void);
void arch_init_percpu(void);
void subsys_init_percpu(void);
void module_init_percpu(void);
void device_init_percpu(void);

#endif
