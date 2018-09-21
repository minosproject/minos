#ifndef __MINOS_PLATFORM_H__
#define __MINOS_PLATFORM_H__

#include <minos/compiler.h>

struct platform {
	char *name;
	int (*cpu_on)(unsigned long cpu, unsigned long entry);
	int (*cpu_off)(unsigned long cpu);
	void (*system_reboot)(int mode, const char *cmd);
	void (*system_shutdown)(void);
	int (*time_init)(void);
};

extern struct platform *platform;

#define DEFINE_PLATFORM(pl)	\
	static struct platform *__platform__##pl __used \
		__section(".__platform") = &pl

#endif
