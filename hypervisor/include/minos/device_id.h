#ifndef _MINOS_DEVICE_ID_H_
#define _MINOS_DEVICE_ID_H_

#include <minos/init.h>

struct module_id {
	char name[32];
	char type[32];
	char cmpstr[32];
	void *data;
};

#define MINOS_MODULE_DECLARE(mname, mn, init_fn) \
	static const struct module_id __used \
	module_match_##mname __section(.__vmodule) = { \
		.name = mn, \
		.type = "vmodule", \
		.data = init_fn, \
	}

#define IRQCHIP_DECLARE(mname, mn, irqchip) \
	static const struct module_id __used \
	module_match_##mname __section(.__irqchip) = { \
		.name = mn, \
		.type = "irqchip", \
		.data = irqchip, \
	}

#define MMUCHIP_DECLARE(mname, mn, mmuchip) \
	static const struct module_id __used \
	module_match_##mname __section(.__mmuchip) = { \
		.name = mn, \
		.type = "mmuchip", \
		.data = mmuchip, \
	}

#endif
