#ifndef _MVISOR_DEVICE_ID_H_
#define _MVISOR_DEVICE_ID_H_

#include <mvisor/init.h>

struct module_id {
	char name[32];
	char type[32];
	char cmpstr[32];
	void *fn;
};

#define VMM_MODULE_DECLARE(mname, mn, t, init_fn) \
	static const struct module_id vmm_module_match_##mname \
	__section(__vmm_module) = { \
		.name = mn, \
		.type = t, \
		.fn = init_fn, \
	}

#endif
