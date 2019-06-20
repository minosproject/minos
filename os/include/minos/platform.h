#ifndef __MINOS_PLATFORM_H__
#define __MINOS_PLATFORM_H__

#include <minos/compiler.h>

#ifdef CONFIG_VIRT
struct vm;
#endif

struct platform {
	const char *name;
	int (*cpu_on)(unsigned long cpu, unsigned long entry);
	int (*cpu_off)(unsigned long cpu);
	void (*system_reboot)(int mode, const char *cmd);
	void (*system_shutdown)(void);
	int (*system_suspend)(void);
	int (*time_init)(void);
#ifdef CONFIG_VIRT
	int (*setup_hvm)(struct vm *vm, void *data);
#endif
	int (*platform_init)(void);
	void (*parse_mem_info)(void);
};

extern struct platform *platform;

#define DEFINE_PLATFORM(pl)	\
	static struct platform *__platform__##pl __used \
		__section(".__platform") = &pl

extern void platform_set_to(const char *name);

#endif
