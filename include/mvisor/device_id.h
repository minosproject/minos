#ifndef _MVISOR_DEVICE_ID_H_
#define _MVISOR_DEVICE_ID_H_

struct irq_chip_id {
	char name[32];
	char type[32];
	char compatible[128];
	int (*init)(void);
	void *data;
};

#endif
