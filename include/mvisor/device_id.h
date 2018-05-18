#ifndef _MVISOR_DEVICE_ID_H_
#define _MVISOR_DEVICE_ID_H_

#include <mvisor/init.h>

struct module_id {
	char name[32];
	char type[32];
	char cmpstr[32];
	void *data;
};

#define MVISOR_MODULE_DECLARE(mname, mn, init_fn) \
	static const struct module_id __used \
	mvisor_module_match_##mname __section(.__mvisor_module) = { \
		.name = mn, \
		.type = "mvisor_module", \
		.data = init_fn, \
	}

#define IRQCHIP_DECLARE(mname, mn, irqchip) \
	static const struct module_id __used \
	mvisor_module_match_##mname __section(.__mvisor_irqchip) = { \
		.name = mn, \
		.type = "irqchip", \
		.data = irqchip, \
	}

#define MMUCHIP_DECLARE(mname, mn, mmuchip) \
	static const struct module_id __used \
	mvisor_module_match_##mname __section(.__mvisor_mmuchip) = { \
		.name = mn, \
		.type = "mmuchip", \
		.data = mmuchip, \
	}

#endif
